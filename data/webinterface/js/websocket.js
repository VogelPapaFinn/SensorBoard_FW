const gateway = `ws://${window.location.hostname}/ws`;
let websocket;

function initWebSocket() {
    console.log('Trying to establish a connection with the ESP32 using a Websocket...');

    websocket = new WebSocket(gateway);
    websocket.onopen = (event) => {
        console.log("Websocket connection established")
        requestFetchSensors()
    }

    websocket.onmessage = (event) => {
        console.log('Received message from ESP32: %s', event.data)
        const json = JSON.parse(event.data)
        if(json == null) {
            console.log("Couldn't parse to JSON")
            return
        }

        if(json.type === "fetch-sensors") {
            responseFetchSensors(json)
        }

        else if(json.type === "update-sensors") {
            const timestamp = new Date().getTime();

            json.sensors.forEach(sensor => {
                const series = sensorSeriesMap[sensor.id];
                if (series) {
                    series.append(timestamp, parseFloat(sensor.value));
                }
            });
        }
    };

    websocket.onclose = () => {
        console.log('Connection was closed. Reconnection attempt in 2 Seconds');
        setTimeout(initWebSocket, 2000);
    };
}

// Start der Anwendung
window.addEventListener('load', initWebSocket);