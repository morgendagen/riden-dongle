from flask import Flask, jsonify
import time
from flask import send_from_directory

app = Flask(__name__)

out_on = False


@app.route("/set_v", methods=["POST"])
def setv():
    time.sleep(1)
    return "OK", 200


@app.route("/set_i", methods=["POST"])
def seti():
    time.sleep(1)
    return "OK", 200


# time series: https://observablehq.com/@geofduf/simple-dashboard-line-charts

i_out = 0.0
v_out = 0.0


@app.route("/status", methods=["GET"])
def status():
    time.sleep(1)
    global i_out, v_out
    if out_on:
        if i_out < 10.0:
            i_out += 0.1
        else:
            i_out -= 0.1
        v_out = 13.800 - (i_out * 0.01)
    else:
        i_out = 0.0
        v_out = 0.0

    return (
        jsonify(
            {
                "out_on": out_on,
                "set_v": 13.500,
                "set_c": 0.100,
                "out_v": v_out,
                "out_c": i_out,
                "batt_mode": False,
                "cvmode": True,
                "prot": "None",
                "batt_v": 12.600,
                "ext_t_c": None,
                "int_t_c": 36.00,
                "ah": 15.000,
                "wh": 0.200,
                "max_v": 40,
                "max_c": 10
            }
        ),
        200,
    )


@app.route("/toggle_out", methods=["GET"])
def toggle_out():
    global out_on
    out_on = not out_on
    return status()


@app.route("/", methods=["GET"])
def root():
    return send_from_directory(".", "mockup.html")


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=3000)
