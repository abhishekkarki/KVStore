// js/app.js

let client;
let isSubscribed = false;

// Configuration for InfluxDB
const INFLUXDB_URL = "http://192.168.0.105:8086";
const INFLUXDB_TOKEN = "my-token";       // Replace with your InfluxDB token
const INFLUXDB_ORG = "my-org";
const INFLUXDB_BUCKET = "my-bucket";

// Improved command parsing function
function parseCommand(commandStr) {
    const regex = /[^\s"']+|"([^"]*)"|'([^']*)'/g;
    const args = [];
    let match;
    while ((match = regex.exec(commandStr)) !== null) {
        if (match[1]) {
            args.push(match[1]);
        } else if (match[2]) {
            args.push(match[2]);
        } else {
            args.push(match[0]);
        }
    }
    return args;
}

// Function to execute publish command
function executePublish() {
    const commandInput = document.getElementById('publishCommand').value;
    const args = parseCommand(commandInput);

    // Expected command format:
    // mosquitto_pub -h <host> -t "<topic>" -m "<message>"
    if (args[0] !== 'mosquitto_pub') {
        alert('Invalid publish command. Please start with mosquitto_pub.');
        return;
    }

    // Parse arguments
    let host = 'localhost';
    let topic = '';
    let message = '';
    for (let i = 1; i < args.length; i++) {
        if (args[i] === '-h') {
            host = args[++i];
        } else if (args[i] === '-t') {
            topic = args[++i];
        } else if (args[i] === '-m') {
            message = args[++i];
        }
    }

    // Connect to MQTT broker if not already connected
    if (!client || client.options.hostname !== host) {
        client = connectClient(host);
    }

    // Publish the message
    client.publish(topic, message);
    appendResponse(`Published to ${topic}: ${message}`);
}

// Function to execute subscribe command
function executeSubscribe() {
    const commandInput = document.getElementById('subscribeCommand').value;
    const args = parseCommand(commandInput);

    // Expected command format:
    // mosquitto_sub -h <host> -t "<topic>"
    if (args[0] !== 'mosquitto_sub') {
        alert('Invalid subscribe command. Please start with mosquitto_sub.');
        return;
    }

    // Parse arguments
    let host = 'localhost';
    let topic = '';
    for (let i = 1; i < args.length; i++) {
        if (args[i] === '-h') {
            host = args[++i];
        } else if (args[i] === '-t') {
            topic = args[++i];
        }
    }

    // Connect to MQTT broker if not already connected
    if (!client || client.options.hostname !== host) {
        client = connectClient(host);
    }

    // Subscribe to the topic
    client.subscribe(topic, function (err) {
        if (!err) {
            appendResponse(`Subscribed to ${topic}`);
            isSubscribed = true;
        } else {
            appendResponse(`Subscription error: ${err.message}`);
        }
    });
}

// Connect to the MQTT broker
function connectClient(host) {
    const options = {
        hostname: host,
        port: 9001, // WebSocket port for MQTT over WebSockets
        path: '/mqtt',
        protocol: 'ws',
    };

    const client = mqtt.connect(options);

    client.on('connect', function () {
        appendResponse(`Connected to MQTT broker at ${host}`);
    });

    client.on('message', function (topic, message) {
        const msgStr = message.toString();
        appendResponse(`Received message on ${topic}: ${msgStr}`);

        // Attempt to parse the message as JSON
        try {
            const data = JSON.parse(msgStr);
            if (data.error && data.start_timestamp && data.end_timestamp) {
                // This indicates measurements not found locally
                // Automatically query InfluxDB for the given range
                appendResponse(`Data not available locally, querying InfluxDB for range ${data.start_timestamp} - ${data.end_timestamp}...`);
                fetchInfluxDBRange(data.start_timestamp, data.end_timestamp);
            }
        } catch (e) {
            // Not a JSON message or doesn't contain the fields we need
        }
    });

    client.on('error', function (error) {
        appendResponse(`Connection error: ${error.message}`);
    });

    return client;
}

// Append responses to the response area
function appendResponse(message) {
    const responseArea = document.getElementById('responseArea');
    const messageElement = document.createElement('div');
    messageElement.textContent = message;
    responseArea.appendChild(messageElement);
    // Auto-scroll to the bottom
    responseArea.scrollTop = responseArea.scrollHeight;
}

// Function to query InfluxDB for a given timestamp range
async function fetchInfluxDBRange(startTimestamp, endTimestamp) {
    const startRFC = new Date(startTimestamp * 1000).toISOString();
    const endRFC = new Date(endTimestamp * 1000).toISOString();

    const url = `${INFLUXDB_URL}/api/v2/query?org=${INFLUXDB_ORG}`;
    const token = INFLUXDB_TOKEN;

    const query = `
        from(bucket:"${INFLUXDB_BUCKET}")
          |> range(start: ${startRFC}, stop: ${endRFC})
          |> filter(fn: (r) => r["_measurement"] == "temperature_esp")
          |> filter(fn: (r) => r["_field"] == "temperature")
    `;

    const queryPayload = { query: query };

    try {
        const response = await fetch(url, {
            method: "POST",
            headers: {
                "Authorization": `Token ${token}`,
                "Content-Type": "application/json",
                // Not relying on Accept: application/json since the endpoint returns CSV
                "Accept": "application/csv"
            },
            body: JSON.stringify(queryPayload)
        });

        if (!response.ok) {
            const errorText = await response.text();
            appendResponse(`InfluxDB query error: ${errorText}`);
            return;
        }

        const csvData = await response.text();
        appendResponse(`Received CSV data from InfluxDB for range ${startTimestamp}-${endTimestamp}. Parsing CSV...`);

        const measurements = parseInfluxDBCSV(csvData);

        if (measurements.length > 0) {
            appendResponse(`Retrieved ${measurements.length} measurements from InfluxDB:`);
            measurements.forEach(m => appendResponse(`ts=${m.timestamp}, val=${m.temperature}`));
        } else {
            appendResponse("No measurements found in InfluxDB for the given range.");
        }

    } catch (error) {
        appendResponse(`Error fetching data from InfluxDB: ${error.message}`);
    }
}

function parseInfluxDBCSV(csvData) {
    const lines = csvData.split('\n');
    let measurements = [];
    let timeIndex = -1;
    let valueIndex = -1;
    let headerFound = false;

    for (const line of lines) {
        if (line.startsWith('#') || line.trim() === '') {
            // Skip annotation and empty lines
            continue;
        }

        const cols = line.split(',');
        // Identify header line
        if (!headerFound && cols.includes('_time') && cols.includes('_value')) {
            // This line should be something like: ,result,table,_time,_value,_field,_measurement
            timeIndex = cols.indexOf('_time');
            valueIndex = cols.indexOf('_value');
            headerFound = true;
            continue;
        }

        // Now parse data lines
        // Check if this line has enough columns
        if (headerFound && timeIndex !== -1 && valueIndex !== -1 && cols.length > valueIndex) {
            const timeStr = cols[timeIndex];
            const valueStr = cols[valueIndex];
            if (timeStr && valueStr) {
                const timestamp = Math.floor(new Date(timeStr).getTime() / 1000);
                const temperature = parseFloat(valueStr);
                measurements.push({ timestamp, temperature });
            }
        }
    }

    return measurements;
}




// curl -v -X POST "http://192.168.0.105:8086/api/v2/query?org=my-org" \
//   -H "Authorization: Token my-token" \
//   -H "Content-Type: application/json" \
//   -H "Accept: application/json" \
//   -d '{
//     "query": "from(bucket:\"my-bucket\") |> range(start:-23h) |> filter(fn: (r) => r._measurement == \"temperature_esp\") |> filter(fn: (r) => r._field == \"temperature\")"
//   }'