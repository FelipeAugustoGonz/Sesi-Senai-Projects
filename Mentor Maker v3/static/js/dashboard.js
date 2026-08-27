// ─── HELPERS ──────────────────────────────────────────────────────────────────
 
function formatHora(timestamp) {
    if (!timestamp) return "Em andamento";
    return new Date(timestamp * 1000).toLocaleTimeString("pt-BR");
}
 
function formatDuracao(inicio, fim) {
    if (!fim) return "Em andamento";
    const total = Math.max(0, Math.round(fim - inicio));
    const minutos = Math.floor(total / 60);
    const segundos = total % 60;
    return minutos === 0 ? `${segundos}s` : `${minutos}m ${segundos}s`;
}
 
function formatModo(modo) {
    const labels = {
        idle: "Aguardando",
        ouvindo: "Ouvindo",
        indo: "Em deslocamento",
        atendendo: "Atendendo",
        voltando: "Voltando",
    };
    return labels[modo] || modo || "--";
}
 
// ─── HISTÓRICO ────────────────────────────────────────────────────────────────
 
function renderHistorico(historico) {
    const container = document.getElementById("historico-lista");
 
    if (!historico.length) {
        container.className = "history-list empty-state";
        container.textContent = "Nenhum atendimento registrado ainda.";
        return;
    }
 
    container.className = "history-list";
    const itens = [...historico].reverse();
 
    container.innerHTML = itens.map((item) => {
        const finalizado = item.fim !== null;
        return `
            <article class="history-item">
                <div class="history-topline">
                    <strong>Grupo ${item.grupo}</strong>
                    <span class="tag ${finalizado ? "" : "warn"}">${finalizado ? "Finalizado" : "Em andamento"}</span>
                </div>
                <div class="meta-row">
                    <span>Início ${formatHora(item.inicio)}</span>
                    <span>Fim ${formatHora(item.fim)}</span>
                    <span>Duração ${formatDuracao(item.inicio, item.fim)}</span>
                </div>
                <div class="meta-row">
                    <span>Conteúdo: ${item.conteudo || "inicio"}</span>
                    <button class="btn-ver-conversa" data-id="${item.id}" data-grupo="${item.grupo}">
                        💬 Ver conversa
                    </button>
                </div>
            </article>
        `;
    }).join("");
 
    // Listeners para abrir modal de conversa
    container.querySelectorAll(".btn-ver-conversa").forEach((btn) => {
        btn.addEventListener("click", () => {
            abrirConversa(btn.dataset.id, btn.dataset.grupo);
        });
    });
}
 
// ─── FILA ─────────────────────────────────────────────────────────────────────
 
function renderFila(fila) {
    const container = document.getElementById("queue-bars");
 
    if (!fila.length) {
        container.className = "queue-bars empty-state";
        container.textContent = "Sem grupos em espera.";
        return;
    }
 
    container.className = "queue-bars";
 
    // Separa urgentes dos normais e ordena normais por nível/tempo
    const urgentes = fila.filter(item => item.urgente);
    const normais  = fila
        .filter(item => !item.urgente)
        .sort((a, b) => b.nivel !== a.nivel ? b.nivel - a.nivel : a.tempo - b.tempo);
 
    // Urgentes sempre primeiro
    const ordenada = [...urgentes, ...normais];
 
    const NIVEL_MAX = 12;
 
    container.innerHTML = ordenada.map((item) => {
        const nivelSeguro = item.urgente
            ? NIVEL_MAX  // barra cheia para urgente
            : Math.max(1, Math.min(item.nivel, NIVEL_MAX));
 
        const largura = Math.round((nivelSeguro / NIVEL_MAX) * 100);
 
        const labelNivel = item.urgente
            ? "⚠️ URGENTE"
            : `Nível ${item.nivel}`;
 
        return `
            <article class="bar-item">
                <div class="bar-head">
                    <strong>Grupo ${item.grupo}</strong>
                    <span>${labelNivel}</span>
                </div>
                <div class="bar-track">
                    <div class="bar-fill" style="width: ${largura}%"></div>
                </div>
            </article>
        `;
    }).join("");
}
 
// ─── STATS POR GRUPO (banco) ──────────────────────────────────────────────────
 
function renderStatsPorGrupo(dados) {
    const container = document.getElementById("stats-por-grupo");
 
    if (!dados.length) {
        container.className = "empty-state";
        container.textContent = "Nenhum dado ainda.";
        return;
    }
 
    container.className = "";
    container.innerHTML = dados.map((d) => {
        const tempoMedio = d.tempo_medio
            ? `${Math.round(d.tempo_medio)}s`
            : "—";
        const pct = d.total > 0
            ? Math.round((d.finalizados / d.total) * 100)
            : 0;
 
        return `
            <article class="grupo-stat-card">
                <div class="grupo-stat-header">
                    <span class="grupo-stat-label">Grupo ${d.grupo}</span>
                    <span class="grupo-stat-badge">${d.total} atend.</span>
                </div>
                <div class="grupo-stat-row">
                    <span>Finalizados</span>
                    <strong>${d.finalizados} (${pct}%)</strong>
                </div>
                <div class="grupo-stat-row">
                    <span>Tempo médio</span>
                    <strong>${tempoMedio}</strong>
                </div>
                <div class="bar-track" style="margin-top:8px">
                    <div class="bar-fill" style="width:${pct}%; background: var(--sesi-green)"></div>
                </div>
            </article>
        `;
    }).join("");
}
 
// ─── GRÁFICO LINHA DO TEMPO (Chart.js) ───────────────────────────────────────
 
let chartHora = null;
 
function renderChartHora(dados) {
    const ctx = document.getElementById("chart-hora").getContext("2d");
 
    // Monta array com todas as horas 0-23, preenchendo zeros onde não há dado
    const totais = Array(24).fill(0);
    dados.forEach((d) => { totais[d.hora] = d.total; });
 
    const labels = totais.map((_, i) => `${String(i).padStart(2, "0")}h`);
 
    if (chartHora) {
        chartHora.data.datasets[0].data = totais;
        chartHora.update("none");
        return;
    }
 
    chartHora = new Chart(ctx, {
        type: "bar",
        data: {
            labels,
            datasets: [{
                label: "Atendimentos",
                data: totais,
                backgroundColor: "rgba(255, 107, 53, 0.72)",
                borderRadius: 6,
            }]
        },
        options: {
            responsive: true,
            plugins: {
                legend: { display: false },
            },
            scales: {
                y: {
                    beginAtZero: true,
                    ticks: { stepSize: 1 },
                    grid: { color: "rgba(0,0,0,0.06)" }
                },
                x: {
                    grid: { display: false }
                }
            }
        }
    });
 
    // Texto auxiliar acima do gráfico
    const statsContainer = document.getElementById("stats-linha-tempo");
    if (dados.length) {
        const pico = dados.reduce((a, b) => (a.total >= b.total ? a : b));
        statsContainer.className = "";
        statsContainer.textContent =
            `Horário de pico: ${String(pico.hora).padStart(2, "0")}h com ${pico.total} atendimento(s).`;
    } else {
        statsContainer.className = "empty-state";
        statsContainer.textContent = "Nenhum dado ainda.";
    }
}
 
// ─── MODAL DE CONVERSA ────────────────────────────────────────────────────────
 
async function abrirConversa(atendimentoId, grupo) {
    const modal = document.getElementById("modal-conversa");
    const titulo = document.getElementById("modal-titulo");
    const corpo = document.getElementById("modal-corpo");
 
    titulo.textContent = `Conversa — Grupo ${grupo}`;
    corpo.innerHTML = "<p class='empty-state'>Carregando...</p>";
    modal.hidden = false;
 
    try {
        const res = await fetch(`/historico/${atendimentoId}/conversas`);
        const msgs = await res.json();
 
        if (!msgs.length) {
            corpo.innerHTML = "<p class='empty-state'>Nenhuma mensagem registrada neste atendimento.</p>";
            return;
        }
 
        corpo.innerHTML = msgs.map((m) => `
            <div class="msg msg-${m.papel}">
                <span class="msg-autor">${m.papel === "aluno" ? "👦 Aluno" : "🤖 Léia"}</span>
                <p class="msg-texto">${m.mensagem}</p>
                <span class="msg-hora">${formatHora(m.criado_em)}</span>
            </div>
        `).join("");
 
    } catch (e) {
        corpo.innerHTML = "<p class='empty-state'>Erro ao carregar conversa.</p>";
    }
}
 
document.getElementById("modal-fechar").addEventListener("click", () => {
    document.getElementById("modal-conversa").hidden = true;
});
 
document.getElementById("modal-conversa").addEventListener("click", (e) => {
    if (e.target === e.currentTarget) e.currentTarget.hidden = true;
});
 
// ─── LOOP PRINCIPAL ───────────────────────────────────────────────────────────
 
async function carregarDashboard() {
    try {
        const [
            historicoRes,
            resumoRes,
            filaRes,
            statsPorGrupoRes,
            statsHoraRes,
        ] = await Promise.all([
            fetch("/historico"),
            fetch("/resumo"),
            fetch("/fila_display"),
            fetch("/stats/por_grupo"),
            fetch("/stats/linha_tempo"),
        ]);
 
        const historico      = await historicoRes.json();
        const resumo         = await resumoRes.json();
        const filaDisplay    = await filaRes.json();
        const statsPorGrupo  = await statsPorGrupoRes.json();
        const statsHora      = await statsHoraRes.json();
 
        // Resumo numérico
        document.getElementById("stat-total").textContent        = resumo.total_atendimentos;
        document.getElementById("stat-andamento").textContent    = resumo.em_andamento;
        document.getElementById("stat-finalizados").textContent  = resumo.finalizados;
        document.getElementById("stat-tempo-medio").textContent  = `${resumo.tempo_medio_segundos}s`;
 
        // Status ao vivo
        document.getElementById("live-modo").textContent     = formatModo(resumo.modo);
        document.getElementById("live-escuta").textContent   = resumo.ouvindo ? "Ativa" : "Pausada";
        document.getElementById("live-grupo").textContent    = resumo.grupo_atual ?? "--";
        document.getElementById("live-urgente").textContent  = resumo.urgente ?? "Nenhum";
        document.getElementById("live-conteudo").textContent = resumo.conteudo;
 
        renderFila(filaDisplay || []);
        renderHistorico(historico || []);
        renderStatsPorGrupo(statsPorGrupo || []);
        renderChartHora(statsHora || []);
 
    } catch (e) {
        console.error("Erro ao carregar dashboard:", e);
    }
}
 
carregarDashboard();
setInterval(carregarDashboard, 1000);