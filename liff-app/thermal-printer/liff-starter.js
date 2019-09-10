const THERMAL_PRINTER_SERVICE_UUID = "4a40d898-cb8a-49fa-9471-c16aaef23b56";
const COMMAND_CHARACTERISTIC = "2064E034-2E6A-40E1-9682-20742CAA9987";
const FLOW_CONTROL_CHARACTERISTIC_UUID = "F2F31CFB-322C-47C5-B7F9-997394B9568C";
const PSDI_SERVICE_UUID = "E625601E-9E55-4597-A598-76018A0D293D";
const PSDI_CHARACTERISTIC_UUID = "26E2B12B-85F0-4F3F-9FDD-91D114270E6E";

const PAPER_WIDTH = 384;
const DOTS_PER_MM = 8;
//const PAPER_HEIGHT = 500;
const BUFFER_HEIGHT = 100;

const CMD_RESET       = 0x00;
const CMD_TEST        = 0x01;
const CMD_TESTPAGE    = 0x02;
const CMD_SET_DEFAULT = 0x03;
const CMD_WAKE        = 0x04;
const CMD_SLEEP       = 0x05;
const CMD_FEED        = 0x06;
const CMD_FEED_ROWS   = 0x07;
const CMD_BITMAP_WRITE  = 0x10;
const CMD_BITMAP_FLUSH  = 0x11;
const CMD_TEXT_PRINT    = 0x20;
const CMD_TEXT_PRINTLN  = 0x21;
const CMD_TEXT_SIZE     = 0x22;
const CMD_TEXT_STYLE    = 0x23;
const CMD_TEXT_JUSTIFY  = 0x24;

const deviceUUIDSet = new Set();
const connectedUUIDSet = new Set();
const connectingUUIDSet = new Set();
const flowControlDeviceIdSet = new Set();

let logNumber = 1;

Object.defineProperty(Array.prototype, 'flatMap', {
  value: function (f, self) {
    self = self || this;
    return this.reduce(function (ys, x) {
      return ys.concat(f.call(self, x));
    }, []);
  },
  enumerable: false,
});

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
    onScreenLog("Check availablity: " + isAvailable);

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
    onScreenLog('detect: ' + device.id);

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
        connectDevice(device);
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
        device.gatt.connect().then(async () => {
            updateConnectionStatus(device, 'connected');
            connectingUUIDSet.delete(device.id);
            await toggleNotification(device, true);
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
    template.querySelector('.device-card-name').innerText = device.name;

    // Device disconnect button
    template.querySelector('.device-disconnect').addEventListener('click', async () => {
        onScreenLog('Clicked disconnect button');
        await toggleNotification(device, false);
        device.gatt.disconnect();
    });

    // Display LINE Profile refresh button
    template.querySelector('.display-line-refresh').addEventListener('click', () => {
        onScreenLog('Clicked display line profile refresh button');
        refreshImageDisplay(device, getProfileCanvas(device), "upload-profile-progress")
            .catch(e => onScreenLog(`ERROR on refreshImageDisplay(): ${e}\n${e.stack}`));
    });

    // Display text refresh button
    template.querySelector('.display-text-refresh').addEventListener('click', () => {
        onScreenLog('Clicked display text refresh button');
        refreshTextDisplay(device)
            .catch(e => onScreenLog(`ERROR on refreshTextDisplay(): ${e}\n${e.stack}`));
    });

    // Display image refresh button
    template.querySelector('.display-image-refresh').addEventListener('click', () => {
        onScreenLog('Clicked display image refresh button');
        refreshImageDisplay(device, getImageCanvas(device), "upload-image-progress")
            .catch(e => onScreenLog(`ERROR on refreshImageDisplay(): ${e}\n${e.stack}`));
    });

    // Profile image size form
    template.querySelector('.value-profile-image-size').addEventListener('change', event => {
        onScreenLog(`Changed profile image size: ${event.target.value}`);
        renderProfileToCanvas(device)
            .catch(e => onScreenLog(`ERROR on renderProfileToCanvas(): ${e}\n${e.stack}`));
    });

    // Add input form button
    template.querySelector('.add-input').addEventListener('click', () => {
        onScreenLog('Clicked add input button');
        const formText = template.querySelector('.form-text-command').cloneNode(true);
        template.querySelector('.form-command-buttons').before(formText);
    });
    template.querySelector('.add-qr').addEventListener('click', () => {
        onScreenLog('Clicked add QR button');
        const formQR = template.querySelector('.form-qr-command').cloneNode(true);
        const qrcode = new QRCode(formQR.querySelector('.view-qr'), {
            text: "",
            width: 128,
            height: 128,
            colorDark : "#000000",
            colorLight : "#ffffff",
            correctLevel : QRCode.CorrectLevel.M
        });
        formQR.querySelector('.value-input-qr').addEventListener('change', e => {
            qrcode.clear();
            qrcode.makeCode(e.target.value);
        });
        formQR.style.display = 'block';
        template.querySelector('.form-command-buttons').before(formQR);
    });

    // Image processing
    template.querySelector('.image-input').addEventListener("change", event => {
        const reader = new FileReader();
        reader.readAsDataURL(event.target.files[0]);
        reader.onload = () => {
            onScreenLog('Read file completed.');
            renderImageToCanvas(device, reader.result);
        };
    });

    // Command buttons
    template.querySelector('.button-cmd-feed').addEventListener("click", async event => {
        const value = parseInt(template.querySelector('.value-cmd-feed').value);
        await sendCommand(device, [CMD_FEED, value])
            .catch(e => onScreenLog(`ERROR on sendCommand(): ${e}\n${e.stack}`));
    });
    template.querySelector('.button-cmd-feed-row').addEventListener("click", async event => {
        const value = parseInt(template.querySelector('.value-cmd-feed-row').value);
        await sendCommand(device, [CMD_FEED_ROWS, value])
            .catch(e => onScreenLog(`ERROR on sendCommand(): ${e}\n${e.stack}`));
    });

    // Tabs
    ['line', 'text', 'image', 'settings', 'commands', 'status'].map(key => {
        const tab = template.querySelector(`#nav-${key}-tab`);
        const nav = template.querySelector(`#nav-${key}`);

        tab.id = `nav-${key}-tab-${device.id}`;
        nav.id = `nav-${key}-${device.id}`;

        tab.href = '#' + nav.id;
        tab.setAttribute('aria-controls', nav.id);
        nav.setAttribute('aria-labelledby', tab.id);
    });
    ['advanced'].map(key => {
        const btn = template.querySelector(`#menu-${key}-btn`);
        const menu = template.querySelector(`#menu-${key}`);

        btn.id = `menu-${key}-btn-${device.id}`;
        menu.id = `menu-${key}-${device.id}`;

        btn.setAttribute('aria-controls', menu.id);
        btn.setAttribute('data-target', `#${menu.id}`);
    });

    // Remove existing same id card
    const oldCardElement = getDeviceCard(device);
    if (oldCardElement && oldCardElement.parentNode) {
        oldCardElement.parentNode.removeChild(oldCardElement);
    }

    document.getElementById('device-cards').appendChild(template);
    onScreenLog('Device card initialized: ' + device.name);

    // Render profile information
    renderProfileToCanvas(device)
        .catch(e => onScreenLog(`ERROR on renderProfileToCanvas(): ${e}\n${e.stack}`));
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
        document.getElementById(device.id).classList.add('active');
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

async function toggleNotification(device, state) {
    if (!connectedUUIDSet.has(device.id)) {
        window.alert('Please connect to a device first');
        onScreenLog('Please connect to a device first.');
        return;
    }

    const characteristic = await getCharacteristic(
        device, THERMAL_PRINTER_SERVICE_UUID, FLOW_CONTROL_CHARACTERISTIC_UUID);

    if (!state) {
        // Stop notification
        await stopNotification(characteristic, notificationCallback);
    } else {
        // Start notification
        await enableNotification(characteristic, notificationCallback);
    }
}

async function enableNotification(characteristic, callback) {
    const device = characteristic.service.device;
    characteristic.addEventListener('characteristicvaluechanged', callback);
    await characteristic.startNotifications();
    onScreenLog('Notifications STARTED ' + characteristic.uuid + ' ' + device.id);
}

async function stopNotification(characteristic, callback) {
    const device = characteristic.service.device;
    characteristic.removeEventListener('characteristicvaluechanged', callback);
    await characteristic.stopNotifications();
    onScreenLog('Notifications STOPPED　' + characteristic.uuid + ' ' + device.id);
}

function notificationCallback(e) {
    onScreenLog(`Notify ${e.target.uuid}: ${e.target.value.getInt8()}`);
    const queueFull = e.target.value.getInt8();
    if (queueFull > 0) {
        flowControlDeviceIdSet.add(e.target.service.device.id);
    } else {
        flowControlDeviceIdSet.delete(e.target.service.device.id);
    }
}

async function renderProfileToCanvas(device) {
    const profile = await liff.getProfile();
    onScreenLog(`Profile: ${profile.displayName} ${profile.statusMessage} ${profile.pictureUrl}`);

    const canvas = getProfileCanvas(device);
    if (!canvas.getContext) {
        onScreenLog("Canvas is not supported on this device.");
        return;
    }

    updateDeviceProgress(device, 'upload-profile-progress', 0);
    canvas.width = PAPER_WIDTH;
    canvas.height = getPrintAreaHeight(device);

    const ctx = canvas.getContext('2d');
    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, PAPER_WIDTH, canvas.height);

    let imageWidth;
    switch (getProfileCommandForm(device).querySelector('.value-profile-image-size').value) {
        case 'F':
            imageWidth = PAPER_WIDTH / 2;
            break;
        case 'L':
            imageWidth = PAPER_WIDTH / 2 - 20;
            break;
        case 'M':
            imageWidth = PAPER_WIDTH / 2 - 40;
            break;
        case 'S':
            imageWidth = PAPER_WIDTH / 2 - 60;
            break;
        default:
            imageWidth = 0;
            break;
    }

    if (imageWidth > 0 && profile.pictureUrl) {
        await drawImageFromURL(
            canvas, profile.pictureUrl,
            (PAPER_WIDTH - imageWidth) / 2, 0, imageWidth, imageWidth);
    }

    await drawImageFromURL(
        canvas, "./LINE_APP.png", (PAPER_WIDTH - 100) / 2, imageWidth + 175, 100, 100, 1);

    const offsetY = imageWidth + 75;
    ctx.strokeStyle = 'black';
    ctx.fillStyle = 'black';
    ctx.textAlign = "center";
    ctx.font = "bold 40px Verdana";
    ctx.fillText(profile.displayName, PAPER_WIDTH / 2, offsetY, PAPER_WIDTH);
    ctx.font = "bold 30px Verdana";
    ctx.fillText(profile.statusMessage || "", PAPER_WIDTH / 2, offsetY + 50, PAPER_WIDTH);

    // threshold for text
    const image = ctx.getImageData(0, 0, PAPER_WIDTH, canvas.height);
    const dithered = new CanvasDither().threshold(image, 190);
    ctx.putImageData(dithered, 0, 0);
}

function renderImageToCanvas(device, dataUrl) {
    const image = new Image();
    image.crossOrigin = "Anonymous";
    image.onload = () => {
        onScreenLog(`Image loaded: ${image.width}x${image.height}`);
        updateDeviceProgress(device, 'upload-image-progress', 0);

        const canvas = getImageCanvas(device);
        if (!canvas.getContext) {
            onScreenLog("Canvas is not supported on this device.");
            return;
        }

        canvas.width = PAPER_WIDTH;
        canvas.height = getPrintAreaHeight(device);

        const ctx = canvas.getContext('2d');
        ctx.fillStyle = 'white';
        ctx.fillRect(0, 0, PAPER_WIDTH, canvas.height);

        const ratioWidth = PAPER_WIDTH / image.width;
        const ratioHeight = canvas.height / image.height;

        if (ratioHeight > ratioWidth) {
            const height = Math.floor(image.height * ratioWidth);
            //const y = Math.floor((canvas.height - height) / 2);
            canvas.height = height;
            drawImage(canvas, image, 0, 0, PAPER_WIDTH, height);
        } else {
            const width = Math.floor(image.width * ratioHeight);
            //const x = Math.floor((DISPLAY_CANVAS_WIDTH - width) / 2);
            drawImage(canvas, image, 0, 0, width, canvas.height);
        }
    };
    image.src = dataUrl;
}

function drawImageFromURL(canvas, dataUrl, x, y, width, height, mode=0) {
    return new Promise((resolve, reject) => {
        const image = new Image();
        image.crossOrigin = "Anonymous";
        image.onload = () => {
            onScreenLog(`Image loaded: ${image.width}x${image.height}`);
            drawImage(canvas, image, x, y, width, height, mode);
            resolve();
        };
        image.src = dataUrl;
    });
}

function drawImage(canvas, image, x, y, width, height, mode=0) {
    if (!canvas.getContext) {
        onScreenLog("Canvas is not supported on this device.");
        return;
    }

    const ctx = canvas.getContext('2d');
    ctx.drawImage(image, x, y, width, height);

    // apply dither
    const source = ctx.getImageData(x, y, width, height);
    let dithered;
    if (mode == 1) {
        dithered = new CanvasDither().threshold(source, 200);
    } else {
        dithered = new CanvasDither().atkinson(source);
    }
    ctx.putImageData(dithered, x, y);

    onScreenLog(`Rendered image: ${width}x${height} on ${x}:${y}`);
}

async function refreshImageDisplay(device, canvas, progressBarClass=null) {
    if (!connectedUUIDSet.has(device.id)) {
        window.alert('Please connect to a device first');
        onScreenLog('Please connect to a device first.');
        return;
    }

    const commandCharacteristic = await getCharacteristic(
        device, THERMAL_PRINTER_SERVICE_UUID, COMMAND_CHARACTERISTIC);

    await writeCharacteristic(commandCharacteristic, [CMD_WAKE]);
    await writeCharacteristic(commandCharacteristic, [CMD_SET_DEFAULT]);

    await sendImageData(device, canvas, progressBarClass);

    //await writeCharacteristic(commandCharacteristic, [CMD_FEED, 3]);
    await writeCharacteristic(commandCharacteristic, [CMD_FEED_ROWS, getPrintAreaGap(device)]);
    await writeCharacteristic(commandCharacteristic, [CMD_SLEEP]);
}

async function refreshTextDisplay(device) {
    if (!connectedUUIDSet.has(device.id)) {
        window.alert('Please connect to a device first');
        onScreenLog('Please connect to a device first.');
        return;
    }

    const commandCharacteristic = await getCharacteristic(
        device, THERMAL_PRINTER_SERVICE_UUID, COMMAND_CHARACTERISTIC);

    await writeCharacteristic(commandCharacteristic, [CMD_WAKE]);
    await writeCharacteristic(commandCharacteristic, [CMD_SET_DEFAULT]);

    for (const f of getCommandForms(device)) {
        if (f.classList.contains("form-qr-command")) {
            const textValue = f.querySelector('.value-input-qr').value;
            if (!textValue || textValue.length == 0) {
                continue;
            }

            onScreenLog(`QR: "${textValue}"`);
            await writeCharacteristic(commandCharacteristic, [CMD_FEED, 1]);
            await sendImageData(device, f.querySelector('canvas'));
            await writeCharacteristic(commandCharacteristic, [CMD_FEED, 1]);
            continue;
        }

        const textValue = f.querySelector('.value-input-text').value;
        const size = f.querySelector('.value-font-size').value;
        const style = f.querySelector('.value-font-style').value;
        const justify = f.querySelector('.value-font-justify').value;
        if (!textValue || textValue.length == 0) {
            continue;
        }

        onScreenLog(`Text: "${textValue}" ${size} ${style} ${justify}`);
        await writeCharacteristic(commandCharacteristic, [CMD_TEXT_JUSTIFY, justify.charCodeAt()]);
        await writeCharacteristic(commandCharacteristic, [CMD_TEXT_STYLE, style.charCodeAt()]);
        await writeCharacteristic(commandCharacteristic, [CMD_TEXT_SIZE, size.charCodeAt()]);

        // Usually, Japanese characters in UTF-8 is 3 byte.
        // 3 bytes * 6 chars = 18 bytes + NUL + COMMAND
        for (let i = 0; i < textValue.length; i += 6) {
            const textBytes = Array.from(new TextEncoder("utf-8").encode(textValue.slice(i, i + 6)));
            await writeCharacteristic(
                commandCharacteristic,
                [i + 6 < textValue.length ? CMD_TEXT_PRINT : CMD_TEXT_PRINTLN]
                    .concat(textBytes).concat([0]));
        }
    };

    await writeCharacteristic(commandCharacteristic, [CMD_FEED, 3]);
    await writeCharacteristic(commandCharacteristic, [CMD_SLEEP]);
}

async function sendImageData(device, canvas, progressBarClass=null) {
    const commandCharacteristic = await getCharacteristic(
        device, THERMAL_PRINTER_SERVICE_UUID, COMMAND_CHARACTERISTIC);
    const dummyCharacteristic = await getCharacteristic(
        device, PSDI_SERVICE_UUID, PSDI_CHARACTERISTIC_UUID);

    if (!canvas.getContext) {
        onScreenLog("Canvas is not supported on this device.");
        return;
    }

    const ctx = canvas.getContext('2d');
    const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height).data
        .map((p, i, arr) => (p > 0 || arr[i+3] == 0) ? 0 : 1)
        .filter((p, i) => i % 4 == 0);

    for (const y in [...Array(canvas.height).keys()]) {
        const intY = parseInt(y, 10);
        const row = [...Array(Math.floor(canvas.width / 8)).keys()]
            .map(x => x * 8)
            .map(x => imageData
                .slice(y * canvas.width + x, y * canvas.width + x + 8)
                .reduce((acc, cur) => (acc << 1) | cur, 0))
            .reduce((acc, bitmap, i) => {
                const bufY = intY % BUFFER_HEIGHT;
                if (i % 16 == 0) {
                    acc.push([CMD_BITMAP_WRITE, bufY & 0xff, bufY >> 8, i / 16, bitmap]);
                } else {
                    acc[acc.length - 1].push(bitmap);
                }
                return acc;
            }, []);

        while (flowControlDeviceIdSet.has(device.id)) {
            //onScreenLog(`Queue full on device: ${device.id}`);
            await sleep(500);
        }

        await Promise.all(row.map((command) => {
            //onScreenLog(`${y}: ${command.map(c => c.toString(16)).join(' ')}`);
            return writeCharacteristic(commandCharacteristic, command);
        }));

        if (y % BUFFER_HEIGHT == BUFFER_HEIGHT - 1 || y == canvas.height - 1) {
            const printY = (y % BUFFER_HEIGHT) + 1;
            await writeCharacteristic(
                commandCharacteristic,
                [CMD_BITMAP_FLUSH, printY & 0xff, printY >> 8]);
        }

        if (y % 20 == 0) {
            // dummy read
            await dummyCharacteristic.readValue();
        }

        if (progressBarClass) {
            updateDeviceProgress(device, progressBarClass, Math.floor((intY + 1) / canvas.height * 100));
        }
    }
}

async function sendCommand(device, command) {
    if (!connectedUUIDSet.has(device.id)) {
        window.alert('Please connect to a device first');
        onScreenLog('Please connect to a device first.');
        return;
    }

    const commandCharacteristic = await getCharacteristic(
        device, THERMAL_PRINTER_SERVICE_UUID, COMMAND_CHARACTERISTIC);

    await writeCharacteristic(commandCharacteristic, [CMD_WAKE]);
    await writeCharacteristic(commandCharacteristic, command);
    await writeCharacteristic(commandCharacteristic, [CMD_SLEEP]);
}

function getPrintAreaHeight(device) {
    const menu = getAdvancedMenu(device);
    const height = parseInt(menu.querySelector('.value-paper-height').value);
    const margin = parseInt(menu.querySelector('.value-paper-margin').value);
    onScreenLog(`height: ${height} ${margin} ${height - (margin * 2)}`);
    return (height - (margin * 2)) * DOTS_PER_MM;
}

function getPrintAreaGap(device) {
    const menu = getAdvancedMenu(device);
    const margin = parseInt(menu.querySelector('.value-paper-margin').value);
    const gap = parseInt(menu.querySelector('.value-paper-gap').value);
    onScreenLog(`gap: ${margin} ${gap} ${(margin * 2) + gap}`);
    return ((margin * 2) + gap) * DOTS_PER_MM;
}

async function readCharacteristic(characteristic) {
    const response = await characteristic.readValue().catch(e => {
        flashSDKError(e);
        throw e;
    });
    if (response) {
        const values = new Uint8Array(response.buffer);
        onScreenLog(`Read ${characteristic.uuid}: ${values}`);
        return values;
    } else {
        throw 'Read value is empty?';
    }
}

async function writeCharacteristic(characteristic, command) {
    await characteristic.writeValue(new Uint8Array(command)).catch(e => {
        flashSDKError(e);
        throw e;
    });
    //onScreenLog(`Wrote ${characteristic.uuid}: ${command}`);
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

function getProfileCommandForm(device) {
    return getDeviceCard(device).getElementsByClassName('form-profile-command')[0];
}

function getCommandForms(device) {
    return getDeviceCard(device).getElementsByClassName('form-command');
}

function getAdvancedMenu(device) {
    return document.getElementById(`menu-advanced-${device.id}`);
}

function getImageCanvas(device) {
    return getDeviceCard(device).getElementsByClassName('image-thumbnail')[0];
}

function getProfileCanvas(device) {
    return getDeviceCard(device).getElementsByClassName('profile-preview')[0];
}

function updateDeviceProgress(device, clazz, level) {
    const progressBar = document.getElementById('device-' + device.id).getElementsByClassName(clazz)[0];

    if (level) {
        progressBar.style.width = level + '%';
        progressBar.innerText = level + '%';
        progressBar.setAttribute("aria-valuenow", level);
    } else {
        progressBar.style.width = '0%';
        progressBar.innerText = 'N/A';
        progressBar.setAttribute("aria-valuenow", 0);
    }
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
