const DISPLAY_SERVICE_UUID = "777AD5D4-7355-4682-BF3E-72FE7C70B3CE";
const TEXT_WRITE_CHARACTERISTIC_UUID = "F13DB656-27CF-4E0D-9FC6-61FF3BAEA821";
const TEXT_COLOR_WRITE_CHARACTERISTIC_UUID = "3D008758-2D0A-4A57-A3A0-F66610AA3465";
const CONFIG_CHARACTERISTIC_UUID = "F4452A89-E533-41C5-9716-674F26119D2C";

const deviceUUIDSet = new Set();
const connectedUUIDSet = new Set();
const connectingUUIDSet = new Set();

let logNumber = 1;

function onScreenLog(text) {
    const logbox = document.getElementById('logbox');
    logbox.value += '#' + logNumber + '> ';
    logbox.value += text;
    logbox.value += '\n';
    logbox.scrollTop = logbox.scrollHeight;
    logNumber++;
}

window.onload = () => {
    liff.init(async () => {
        onScreenLog('LIFF initialized');
        renderVersionField();

        await liff.initPlugins(['bluetooth']);
        onScreenLog('BLE plugin initialized');

        checkAvailablityAndDo(() => {
            onScreenLog('Finding devices...');
            findDevice();
        });
    }, e => {
        flashSDKError(e);
        onScreenLog(`ERROR on getAvailability: ${e}`);
    });
}

async function checkAvailablityAndDo(callbackIfAvailable) {
    const isAvailable = await liff.bluetooth.getAvailability().catch(e => {
        flashSDKError(e);
        onScreenLog(`ERROR on getAvailability: ${e}`);
        return false;
    });
    // onScreenLog("Check availablity: " + isAvailable);

    if (isAvailable) {
        document.getElementById('alert-liffble-notavailable').style.display = 'none';
        callbackIfAvailable();
    } else {
        document.getElementById('alert-liffble-notavailable').style.display = 'block';
        setTimeout(() => checkAvailablityAndDo(callbackIfAvailable), 1000);
    }
}

// Find LINE Things device using requestDevice()
async function findDevice() {
    const device = await liff.bluetooth.requestDevice().catch(e => {
        flashSDKError(e);
        onScreenLog(`ERROR on requestDevice: ${e}`);
        throw e;
    });
    // onScreenLog('detect: ' + device.id);

    try {
        if (!deviceUUIDSet.has(device.id)) {
            deviceUUIDSet.add(device.id);
            addDeviceToList(device);
            connectDevice(device);
        } else {
            // TODO: Maybe this is unofficial hack > device.rssi
            document.querySelector(`#${device.id} .rssi`).innerText = device.rssi;
        }

        checkAvailablityAndDo(() => setTimeout(findDevice, 100));
    } catch (e) {
        onScreenLog(`ERROR on findDevice: ${e}\n${e.stack}`);
    }
}

// Add device to found device list
function addDeviceToList(device) {
    onScreenLog('Device found: ' + device.name);

    const deviceList = document.getElementById('device-list');
    const deviceItem = document.getElementById('device-list-item').cloneNode(true);
    deviceItem.setAttribute('id', device.id);
    deviceItem.querySelector(".device-id").innerText = device.id;
    deviceItem.querySelector(".device-name").innerText = device.name;
    deviceItem.querySelector(".rssi").innerText = device.rssi;
    deviceItem.classList.add("d-flex");
    deviceItem.addEventListener('click', () => {
        deviceItem.classList.add("active");
        try {
            connectDevice(device);
        } catch (e) {
            onScreenLog('Initializing device failed. ' + e);
        }
    });
    deviceList.appendChild(deviceItem);
}

// Select target device and connect it
function connectDevice(device) {
    onScreenLog('Device selected: ' + device.name);

    if (!device) {
        onScreenLog('No devices found. You must request a device first.');
    } else if (connectingUUIDSet.has(device.id) || connectedUUIDSet.has(device.id)) {
        onScreenLog('Already connected to this device.');
    } else {
        connectingUUIDSet.add(device.id);
        initializeCardForDevice(device);

        // Wait until the requestDevice call finishes before setting up the disconnect listner
        const disconnectCallback = () => {
            updateConnectionStatus(device, 'disconnected');
            device.removeEventListener('gattserverdisconnected', disconnectCallback);
        };
        device.addEventListener('gattserverdisconnected', disconnectCallback);

        onScreenLog('Connecting ' + device.name);
        device.gatt.connect().then(() => {
            updateConnectionStatus(device, 'connected');
            connectingUUIDSet.delete(device.id);
        }).catch(e => {
            flashSDKError(e);
            onScreenLog(`ERROR on gatt.connect(${device.id}): ${e}`);
            updateConnectionStatus(device, 'error');
            connectingUUIDSet.delete(device.id);
        });
    }
}

// Setup device information card
function initializeCardForDevice(device) {
    const template = document.getElementById('device-template').cloneNode(true);
    const cardId = 'device-' + device.id;

    template.style.display = 'block';
    template.setAttribute('id', cardId);
    template.querySelector('.card > .card-header > .device-name').innerText = device.name;

    // Device disconnect button
    template.querySelector('.device-disconnect').addEventListener('click', () => {
        onScreenLog('Clicked disconnect button');
        device.gatt.disconnect();
    });

    template.querySelector('.button-send').addEventListener('click', () => {
        updateText(device, 0).catch(e => onScreenLog(`ERROR on updateText(): ${e}\n${e.stack}`));
    });

    template.querySelector('.button-pause').addEventListener('click', () => {
        updateText(device, 1).catch(e => onScreenLog(`ERROR on updateText(): ${e}\n${e.stack}`));
    });

    template.querySelector('.button-add-text').addEventListener('click', () => {
        const textElements = template.querySelector('.input-text-row > .col');
        const textElement = textElements.children[textElements.children.length - 1].cloneNode(true);
        textElement.querySelector('.input-display-text').value = "";
        textElements.appendChild(textElement);
    });

    // Remove existing same id card
    const oldCardElement = getDeviceCard(device);
    if (oldCardElement && oldCardElement.parentNode) {
        oldCardElement.parentNode.removeChild(oldCardElement);
    }

    document.getElementById('device-cards').appendChild(template);
    onScreenLog('Device card initialized: ' + device.name);
}

// Update Connection Status
function updateConnectionStatus(device, status) {
    if (status == 'connected') {
        onScreenLog('Connected to ' + device.name);
        connectedUUIDSet.add(device.id);

        const statusBtn = getDeviceStatusButton(device);
        statusBtn.setAttribute('class', 'device-status btn btn-outline-primary btn-sm disabled');
        statusBtn.innerText = "Connected";
        getDeviceDisconnectButton(device).style.display = 'inline-block';
        getDeviceCardBody(device).style.display = 'block';
    } else if (status == 'disconnected') {
        onScreenLog('Disconnected from ' + device.name);
        connectedUUIDSet.delete(device.id);

        const statusBtn = getDeviceStatusButton(device);
        statusBtn.setAttribute('class', 'device-status btn btn-outline-secondary btn-sm disabled');
        statusBtn.innerText = "Disconnected";
        getDeviceDisconnectButton(device).style.display = 'none';
        getDeviceCardBody(device).style.display = 'none';
        document.getElementById(device.id).classList.remove('active');
    } else {
        onScreenLog('Connection Status Unknown ' + status);
        connectedUUIDSet.delete(device.id);

        const statusBtn = getDeviceStatusButton(device);
        statusBtn.setAttribute('class', 'device-status btn btn-outline-danger btn-sm disabled');
        statusBtn.innerText = "Error";
        getDeviceDisconnectButton(device).style.display = 'none';
        getDeviceCardBody(device).style.display = 'none';
        document.getElementById(device.id).classList.remove('active');
    }
}

async function updateText(device, pause) {
    const textForms = getDeviceInputText(device);
    const colorForms = getDeviceInputColor(device);

    let textPayload = []
    let colorPayload = []
    for (let i = 0; i < textForms.length; i++) {
        const textInput = textForms[i].value;
        const colorInput = colorForms[i].value;

        onScreenLog(`${textInput} ${colorInput}`);
        textPayload = textPayload.concat(textInput.split("").map(c => c.charCodeAt()));
        colorPayload = colorPayload.concat(colorInput.repeat(textInput.length).split("").map(c => c.charCodeAt()));
    }
    textPayload = textPayload.concat(["\0"]);
    colorPayload = colorPayload.concat(["\0"]);
    onScreenLog(`${textPayload}`);
    onScreenLog(`${colorPayload}`);

    const textCharacteristic = await getCharacteristic(
        device, DISPLAY_SERVICE_UUID, TEXT_WRITE_CHARACTERISTIC_UUID);
    const colorCharacteristic = await getCharacteristic(
        device, DISPLAY_SERVICE_UUID, TEXT_COLOR_WRITE_CHARACTERISTIC_UUID);
    const configCharacteristic = await getCharacteristic(
        device, DISPLAY_SERVICE_UUID, CONFIG_CHARACTERISTIC_UUID);

    const brightness = parseInt(getDeviceInputBrightness(device).value);
    const rainbow = parseInt(getDeviceInputRainbow(device).value);
    const scroll = parseInt(getDeviceInputScroll(device).value);
    const blink = parseInt(getDeviceInputBlink(device).value);
    await writeCharacteristic(configCharacteristic, [pause, brightness, scroll & 0xff, scroll >> 8, blink & 0xff, blink >> 8, rainbow & 0xff, rainbow >> 8]);

    for (let i = 0; i < textPayload.length; i += 20) {
        await writeCharacteristic(textCharacteristic, textPayload.slice(i, i + 20));
        await writeCharacteristic(colorCharacteristic, colorPayload.slice(i, i + 20));
    }
}

async function readCharacteristic(characteristic) {
    const response = await characteristic.readValue().catch(e => {
        onScreenLog(`Error reading ${characteristic.uuid}: ${e}`);
        throw e;
    });
    if (response) {
        onScreenLog(`Read ${characteristic.uuid}: ${buf2hex(response.buffer)}`);
        const values = new DataView(response.buffer);
        return values;
    } else {
        throw 'Read value is empty?';
    }
}

async function writeCharacteristic(characteristic, command) {
    await characteristic.writeValue(new Uint8Array(command)).catch(e => {
        onScreenLog(`Error writing ${characteristic.uuid}: ${e}`);
        throw e;
    });
    onScreenLog(`Wrote ${characteristic.uuid}: ${command}`);
}

async function getCharacteristic(device, serviceId, characteristicId) {
    const service = await device.gatt.getPrimaryService(serviceId).catch(e => {
        flashSDKError(e);
        throw e;
    });
    const characteristic = await service.getCharacteristic(characteristicId).catch(e => {
        flashSDKError(e);
        throw e;
    });
    onScreenLog(`Got characteristic ${serviceId} ${characteristicId} ${device.id}`);
    return characteristic;
}

function getDeviceCard(device) {
    return document.getElementById('device-' + device.id);
}

function getDeviceCardBody(device) {
    return getDeviceCard(device).getElementsByClassName('card-body')[0];
}

function getDeviceStatusButton(device) {
    return getDeviceCard(device).getElementsByClassName('device-status')[0];
}

function getDeviceDisconnectButton(device) {
    return getDeviceCard(device).getElementsByClassName('device-disconnect')[0];
}

function getDeviceInputText(device) {
    return getDeviceCard(device).getElementsByClassName('input-display-text');
}

function getDeviceInputColor(device) {
    return getDeviceCard(device).getElementsByClassName('input-display-color');
}

function getDeviceInputBrightness(device) {
    return getDeviceCard(device).getElementsByClassName('input-display-brightness')[0];
}

function getDeviceInputRainbow(device) {
    return getDeviceCard(device).getElementsByClassName('input-display-rainbow')[0];
}

function getDeviceInputScroll(device) {
    return getDeviceCard(device).getElementsByClassName('input-display-scroll')[0];
}

function getDeviceInputBlink(device) {
    return getDeviceCard(device).getElementsByClassName('input-display-blink')[0];
}

function renderVersionField() {
    const element = document.getElementById('sdkversionfield');
    const versionElement = document.createElement('p')
        .appendChild(document.createTextNode('SDK Ver: ' + liff._revision));
    element.appendChild(versionElement);
}

function flashSDKError(error){
    window.alert('SDK Error: ' + error.code);
    window.alert('Message: ' + error.message);
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function buf2hex(buffer) { // buffer is an ArrayBuffer
    return Array.prototype.map.call(new Uint8Array(buffer), x => ('00' + x.toString(16)).slice(-2)).join('');
}
