function requestFetchSensors() {
    websocket.send("fetch-sensors");
}

function responseFetchSensors(json) {
    let select = document.getElementById("sensor-select");

    for(const sensor of json.sensors) {
        let opt = document.createElement("option")
        opt.value = sensor.id
        if(sensor.unit !== "") {
            opt.innerHTML = sensor.name + " (" + sensor.unit + ")"
        } else {
            opt.innerHTML = sensor.name
        }
        select.appendChild(opt)
    }
}