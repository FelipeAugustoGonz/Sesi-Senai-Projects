function setText(id, value) {
    const el = document.getElementById(id);

    if (el) {
        el.textContent = value;
    }
}

let mediaRecorder = null;
let audioChunks = [];
let gravando = false;
let streamAtual = null;
let grupoAtual = null;
let modoAtual = "idle";

function atendimentoAtivo() {
    return modoAtual === "atendendo" && grupoAtual !== null;
}

function atualizarControlesAtendimento() {
    const encerrarButton =
        document.getElementById("encerrar-atendimento-button");

    const listenButton =
        document.getElementById("toggle-listen-button");

    if (encerrarButton) {
        encerrarButton.hidden = !atendimentoAtivo();
    }

    if (listenButton) {
        listenButton.disabled = !atendimentoAtivo();
    }

    if (!atendimentoAtivo()) {
        setText("listen-pill", "Aguardando grupo");
    } else if (!gravando) {
        setText("listen-pill", "Atendendo grupo " + grupoAtual);
    }
}

async function atualizarEstadoSistema() {
    try {
        const response =
            await fetch("/estado_sistema");

        const data =
            await response.json();

        grupoAtual = data.grupo_atual;
        modoAtual = data.modo || "idle";

        setText(
            "grupo-urgente",
            data.urgente === null || data.urgente === undefined
                ? "Nenhum"
                : data.urgente
        );

        atualizarControlesAtendimento();
    } catch (error) {
        console.error("Erro ao atualizar estado:", error);
    }
}

async function alternarEscuta() {
    console.log("Clique microfone");

    if (!atendimentoAtivo()) {
        setText(
            "ia-status",
            "Aguardando um grupo em atendimento."
        );
        return;
    }

    if (!gravando) {
        try {
            streamAtual =
                await navigator.mediaDevices.getUserMedia({
                    audio: true
                });

            console.log("Microfone autorizado");

            mediaRecorder =
                new MediaRecorder(streamAtual);

            audioChunks = [];

            mediaRecorder.ondataavailable =
                (event) => {
                    if (event.data.size > 0) {
                        audioChunks.push(event.data);
                    }
                };

            mediaRecorder.onstop =
                async () => {
                    console.log("Gravacao finalizada");

                    const audioBlob =
                        new Blob(audioChunks, {
                            type: "audio/webm"
                        });

                    const formData =
                        new FormData();

                    formData.append(
                        "audio",
                        audioBlob,
                        "audio.webm"
                    );

                    setText(
                        "ia-status",
                        "Transcrevendo..."
                    );

                    try {
                        const response =
                            await fetch("/audio", {
                                method: "POST",
                                body: formData
                            });

                        const data =
                            await response.json();

                        console.log(data);

                        if (response.ok && data.ok) {
                            setText(
                                "ia-pergunta",
                                data.texto
                            );

                            setText(
                                "ia-resposta",
                                data.resposta
                            );

                            setText(
                                "ia-status",
                                "Pronto"
                            );

                            falar(data.resposta);
                        } else {
                            console.log(data.erro);

                            setText(
                                "ia-status",
                                data.erro || "Erro"
                            );
                        }
                    } catch (error) {
                        console.error(error);

                        setText(
                            "ia-status",
                            "Falha"
                        );
                    }

                    // ─── CORREÇÃO: limpa stream e reseta estado ───────────
                    if (streamAtual) {
                        streamAtual
                            .getTracks()
                            .forEach(track => track.stop());
                        streamAtual = null;
                    }

                    mediaRecorder = null;
                    audioChunks = [];
                    gravando = false;

                    // Restaura o botão para o estado inicial
                    document
                        .getElementById(
                            "toggle-listen-button"
                        )
                        .querySelector(
                            ".voice-button-text"
                        )
                        .textContent =
                            "Toque para falar comigo";

                    if (atendimentoAtivo()) {
                        setText(
                            "listen-pill",
                            "Atendendo grupo " + grupoAtual
                        );
                    }
                    // ─────────────────────────────────────────────────────
                };

            mediaRecorder.start();

            gravando = true;

            document
                .getElementById("listen-pill")
                .textContent =
                    "Ouvindo...";

            document
                .getElementById(
                    "toggle-listen-button"
                )
                .querySelector(
                    ".voice-button-text"
                )
                .textContent =
                    "Clique para parar";
        } catch (error) {
            console.error(
                "Erro microfone:",
                error
            );

            alert(
                "Permita acesso ao microfone."
            );
        }
    } else {
        gravando = false;

        mediaRecorder.stop();

        document
            .getElementById("listen-pill")
            .textContent =
                "Processando...";

        document
            .getElementById(
                "toggle-listen-button"
            )
            .querySelector(
                ".voice-button-text"
            )
            .textContent =
                "Toque para falar comigo";
    }
}

function limparTextoFalado(texto) {
    return texto
        .replace(/Â²/g, " ao quadrado ")
        .replace(/\+/g, " mais ")
        .replace(/\-/g, " menos ")
        .replace(/\//g, " dividido por ")
        .replace(/\*/g, " vezes ")
        .replace(/=/g, " igual a ");
}

function falar(texto) {
    speechSynthesis.cancel();

    const textoLimpo =
        limparTextoFalado(texto);

    const fala =
        new SpeechSynthesisUtterance(
            textoLimpo
        );

    fala.lang = "pt-BR";
    fala.rate = 1;

    speechSynthesis.speak(fala);
}

document
    .getElementById(
        "toggle-listen-button"
    )
    .addEventListener(
        "click",
        alternarEscuta
    );

document
    .getElementById(
        "encerrar-atendimento-button"
    )
    .addEventListener(
        "click",
        async function(event) {
            event.preventDefault();
            event.stopPropagation();

            console.log(
                "ENCERRAMENTO SOLICITADO"
            );

            setText(
                "ia-status",
                "Encerrando atendimento..."
            );

            try {
                const response =
                    await fetch(
                        "/encerrar_manual",
                        {
                            method: "POST"
                        }
                    );

                const data =
                    await response.json();

                console.log(data);

                if (response.ok && data.ok) {
                    if (data.proximo_grupo !== null) {
                        setText(
                            "ia-status",
                            "Atendendo grupo " + data.proximo_grupo
                        );
                    } else {
                        setText(
                            "ia-status",
                            "Atendimento encerrado. Aguardando grupo."
                        );
                    }
                } else {
                    setText(
                        "ia-status",
                        data.erro || "Erro ao encerrar atendimento."
                    );
                }

                await atualizarEstadoSistema();
            } catch (error) {
                console.error(
                    "ERRO ENCERRAMENTO:",
                    error
                );

                setText(
                    "ia-status",
                    "Falha ao encerrar atendimento."
                );
            }
        }
    );

atualizarEstadoSistema();
setInterval(atualizarEstadoSistema, 1000);