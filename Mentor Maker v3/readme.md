# Mentor Maker SESI

AI-powered educational assistance system integrating **Flask**, **ESP32**, **Whisper**, and the **Pepper Robot** for real-time classroom support and interactive tutoring.

---

# Overview

The system architecture works like this:

```text
ESP32 → Flask Server → Pepper Robot → AI → Response → Tablet Interface
```

Real workflow:

1. ESP32 detects a group request
2. Flask server manages queue and priorities
3. Pepper requests the next group
4. Pepper navigates to the group
5. Student asks a question using voice input
6. Whisper transcribes the audio
7. The AI generates a response
8. Pepper speaks the answer
9. The session ends and Pepper returns to the base

---

# Main Features

* Real-time educational assistance
* Voice interaction with students
* Queue and priority management
* Emergency request system
* Pepper autonomous movement
* Audio transcription using Whisper
* AI-generated educational responses
* Persistent SQLite database
* Live monitoring dashboard
* Conversation history system
* Web tablet interface
* ESP32 hardware integration

---

# Technologies Used

## Backend

* Python
* Flask
* SQLite
* Requests
* OpenRouter API
* Whisper AI

## Frontend

* HTML5
* CSS3
* JavaScript
* Chart.js

## Hardware

* ESP32
* Pepper Robot
* LEDs
* Potentiometer
* Buzzer
* Push Buttons

---

# Installation

## 1. Python

Install:

* Python 3.10 or 3.11

Make sure to enable:

```text
Add Python to PATH
```

---

## 2. Install Dependencies

```bash
pip install flask requests openai-whisper
```

Optional:

```bash
pip install urllib3
```

---

## 3. Run the Flask Server

```bash
python server.py
```

Expected output:

```text
Running on http://0.0.0.0:5000
```

---

# Pepper Setup

Required:

* Choregraphe
* NAOqi SDK
* Pepper connected to the same Wi-Fi network

Services used:

* ALMotion
* ALTextToSpeech
* qi.PeriodicTask

---

# Network Configuration

All devices must be connected to the same network.

| Device   | Must Access      |
| -------- | ---------------- |
| Notebook | localhost:5000   |
| Pepper   | NOTEBOOK_IP:5000 |
| ESP32    | NOTEBOOK_IP:5000 |

---

# Discover Your Local IP

Windows:

```bash
ipconfig
```

Find:

```text
IPv4 Address: 192.168.x.x
```

Update Pepper configuration:

```python
self.base_url = "http://192.168.x.x:5000"
```

And ESP32:

```cpp
const char* serverUrl = "http://192.168.x.x:5000/update";
```

---

# Project Structure

```text
/project
│
├── server.py
│
├── templates/
│   ├── base.html
│   ├── index.html
│   └── dashboard.html
│
├── static/
│   ├── js/
│   │   ├── home.js
│   │   └── dashboard.js
│   │
│   └── css/
│       └── app.css
│
├── pepper/
│   └── pepper_script.py
│
└── esp32/
    └── esp32_controller.ino
```

---

# System Workflow

## 1. ESP32 Request

ESP32 sends:

```http
/update?grupo=1&nivel=2&urgente=0
```

Server:

* Adds group to queue
* Updates priority
* Stores level in database

---

## 2. Pepper Requests Next Group

Pepper calls:

```http
/next
```

Server response:

```json
{
  "grupo": 1
}
```

System mode changes to:

```text
indo
```

---

## 3. Pepper Navigation

Pepper executes:

```python
ir_para_grupo()
```

Robot:

* Rotates
* Moves to the selected group
* Starts the session

---

## 4. Session Start

Pepper calls:

```http
/atendimento_start?grupo=1
```

Server mode:

```text
atendendo
```

---

## 5. Voice Recognition

Frontend records audio and sends:

```http
POST /audio
```

Server:

* Receives audio
* Uses Whisper transcription
* Extracts text

---

## 6. AI Response

Server sends the question to OpenRouter AI.

Features:

* Educational assistant personality
* Short beginner-friendly explanations
* Automatic fallback responses if API fails

---

## 7. Pepper Response

Pepper speaks the generated response:

```python
tts.say(resposta)
```

---

## 8. Session Ending

Two possible ways:

### Automatic

Pepper calls:

```http
POST /encerrar_manual
```

### Manual

Tablet button triggers:

```http
POST /encerrar_manual
```

---

## 9. Return to Base

Pepper detects:

```text
modo == "voltando"
```

Executes:

```python
voltar_base()
```

Then calls:

```http
POST /retorno_concluido
```

System mode becomes:

```text
ouvindo
```

---

# Dashboard Features

The dashboard provides:

* Live system monitoring
* Queue visualization
* Attendance history
* Group statistics
* Average session duration
* Hourly activity charts
* Conversation history viewer
* Real-time status updates

---

# Database

SQLite stores:

## Tables

### atendimentos

Stores session history.

### conversas

Stores all student and AI messages.

### niveis_grupo

Stores current priority levels.

---

# Hardware Features

## ESP32

* Potentiometer priority control
* Emergency mode button
* LED indicators
* Audio feedback buzzer
* Wi-Fi communication

## Pepper

* Autonomous movement
* Voice interaction
* Speech synthesis
* Group navigation

---

# Important Endpoints

| Endpoint             | Description                |
| -------------------- | -------------------------- |
| `/update`            | ESP32 sends queue updates  |
| `/next`              | Pepper requests next group |
| `/audio`             | Audio transcription        |
| `/pergunta`          | AI question processing     |
| `/historico`         | Session history            |
| `/resumo`            | System summary             |
| `/fila_display`      | Queue visualization        |
| `/stats/por_grupo`   | Group statistics           |
| `/stats/linha_tempo` | Hourly statistics          |

---

# Things to Test Before Demonstrations

## Critical Tests

### Network

* Pepper can access Flask server
* ESP32 can access Flask server

### AI

* API key valid
* Responses generated correctly

### Movement

* Distances calibrated
* Safe navigation

### Audio

* Microphone working
* Whisper transcription accuracy

### Dashboard

* Real-time updates working
* Database persistence working

---

# Common Problems

| Problem                     | Possible Cause              |
| --------------------------- | --------------------------- |
| Pepper not responding       | Wrong IP                    |
| AI not responding           | Invalid API key             |
| Audio transcription failing | Whisper issue               |
| Pepper not returning        | Endpoint not called         |
| System freezing             | State synchronization issue |

---

# Future Improvements

* Smarter queue algorithms
* Better AI prompts
* Improved Pepper navigation
* Timeout system
* Multi-language support
* Computer vision integration
* Real-time classroom analytics

---

# Authors

Developed as an educational robotics and AI integration project using:

* Pepper Robot
* ESP32
* Flask
* Whisper
* OpenRouter AI
* SQLite
* Web Technologies
