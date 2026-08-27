from flask import Flask
import threading

app = Flask(__name__)

@app.route("/")
def index():
    return '''<!DOCTYPE html>

<html>

<head>

    <title>Labdisc Dashboard</title>

    <link rel="stylesheet" href="/static/style.css">

</head>

<body>
    <h1>Labdisc Dashboard</h1>
    <iframe src="https://globilab.com" width="1280" height="720" allow="bluetooth; serial">
    </iframe>
</body>

</html>'''


if __name__ == "__main__":
      app.run(host='0.0.0.0', port=8080, debug=False, threaded=True)
  
