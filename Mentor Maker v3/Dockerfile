FROM python:3.11

WORKDIR /app

# Instala ffmpeg
RUN apt-get update && apt-get install -y ffmpeg

COPY requirements.txt .

RUN pip install --no-cache-dir -r requirements.txt

COPY . .

EXPOSE 5000

CMD ["python", "server.py"]