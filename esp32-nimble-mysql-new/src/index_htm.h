static const char listble_html[] PROGMEM = R"string_literal(
<style>
th {background-color: cadetblue; padding: 10px;}
th, td {text-align: center; border: 1px solid black; padding: 2px}
input::placeholder{color: lightgray;}
h1 {margin-bottom: 10px};
.insert-row{width: 80%; margin: auto;}
.add{ text-align: center;  margin-top: 20px;}
.row {display: flex;flex-wrap: wrap; padding: 10px;}
.row, .selected {background-color: antiquewhite;}
.cell {text-transform: uppercase; text-align: center; flex-grow: 1; width: 33%; padding: 0.8em 1.2em; border: 1px solid #ccc;  border-radius: 6px;}
.address {text-transform: uppercase;}
.flex-ctn {display: flex; flex-direction: column; width: 90%; margin-top: 40px; padding: 15px; border: 1px solid black;}
.tf-cell {width: -webkit-fill-available;}
.readonly {color: lightgray;}
.list-item {height: 40px; width: 90%; padding: 0 0 0 20px; border: 1px solid #ccc;border-radius: 6px; font-size: 16px;}
.ble{background-color: darkcyan; min-width: 20%;}
.content {max-height: 0;overflow: hidden; transition: max-height 0.4s ease-out;}
#collapsible {background-color: transparent;color: #607D8B; padding: 18px;}
#collapsible:after {content: '\002B';color: white;font-weight: bold;float: right;margin-left: 5px;}

.custom-select {width: 10%; min-width:78px}
.select-trigger { padding: 10px; border: 1px solid #888585; cursor: pointer; border-radius: 6px;}
.select-trigger:hover { background-color: #888585a1;}
.options {position: absolute; max-height: 200px;overflow-y: auto; border: 1px solid #ccc;
          border-top: none;width: fit-content; background: #fff; word-wrap: normal;bottom: 33%;right: 8%;}
.option { padding: 10px;cursor: pointer;}
.option:hover { background-color: #f2f2f2;}
.custom-header{width: 100%; text-align: right;margin-right: 0;}
.inline { display: flex;align-items: center;justify-content: center;}
.gray {color: gray;}
span.inline svg {width: 24px; height: 24px;}
tr.gray td svg {fill: gray;}
#nodb {align-self: flex-end; margin: 3px 2.5vw -0 0;}
#ble-table {width: 94%;}

 @media screen and (max-width: 732px) {
    .desktop {display: none;}
  }
</style>

<table id=ble-table class="table hide">
  <thead>
    <tr><th>Device Name</th><th>Device ID</th><th>Address</th><th class=desktop>RSSI</th><th>Battery</th><th class=desktop>Connected</th></tr>
  </thead>
  <tbody id=ble-list>
    <!--This will be filled at runtime-->
  </tbody>
  
</table>
<i class="hide gray" id="nodb">**Values not stored in DB</i>

<div id=ble-sel class="hide">
  <div class="row-wrapper">
    <div class="tf-cell">
      <label for=ble_address class=input-label>MAC Address</label>
      <input type=text placeholder="Device address" id=ble_address class=readonly readonly>
    </div>
    <div class="tf-cell">
      <label for=ble_id class=input-label>ID</label>
      <input list=devices name=ble_id id=ble_id class=list-item >
      <datalist id=devices>
        <!--This will be filled at runtime-->
      </datalist>
    </div>
  </div>
  
  <div class="row-wrapper">
    <div class="tf-cell">
      <label for=ble_name class=input-label>Device Name</label>
      <input type=text name=ble_name id=ble_name>
    </div>
  <div class="tf-cell">
    <label for=connected_to class=input-label>Connected to HUB</label>
      <input list=hubs name=ble_id id=connected_to class=list-item >
      <datalist id=hubs>
        <!--This will be filled at runtime-->
      </datalist>
    </div>
  </div>

  <button id="collapsible">Add connections record for this device</button>
  <div class="content">
    <div class=row-wrapper>
      <input type=text placeholder="Device A, Device B" id=targets>
      <div class="custom-select" id="customSelect">
        <div class="select-trigger" id="selectTrigger">Targets</div>
        <div class="options hide" id="options">
          <!--This will be filled at runtime-->
        </div>
      </div>
    </div>
  </div>
</div>

<div class="btn-bar" style="width:100%">
  <br>
  <a id=list-ble class='btn ble'><span>Get nearby devices</span></a>
  <a id=add-all class='btn ble hide'><span>Add all devices to this hub</span></a>
  <a id=add-ble class='btn ble hide'><span>Add Device</span></a>
  <a id=add-connection class='btn ble hide'><span>Add Connection</span></a>
</div>
)string_literal";


static const char listble_script[] PROGMEM = R"string_literal(
const bleYes = '<svg viewBox="0 0 24 24"><path d="M19,10L17,12L19,14L21,12M14.88,16.29L13,18.17V14.41M13,5.83L14.88,7.71L13,9.58M17.71,7.71L12,2H11V9.58L6.41,5L5,6.41L10.59,12L5,17.58L6.41,19L11,14.41V22H12L17.71,16.29L13.41,12M7,12L5,10L3,12L5,14L7,12Z" /></svg>';
const bleNo = '<svg viewBox="0 0 24 24"><path d="M13,5.83L14.88,7.71L13.28,9.31L14.69,10.72L17.71,7.7L12,2H11V7.03L13,9.03M5.41,4L4,5.41L10.59,12L5,17.59L6.41,19L11,14.41V22H12L16.29,17.71L18.59,20L20,18.59M13,18.17V14.41L14.88,16.29" /></svg>';
const charging = '<svg viewBox="0 0 24 24"><path d="M12 20H4V6H12M12.67 4H11V2H5V4H3.33C2.6 4 2 4.6 2 5.33V20.67C2 21.4 2.6 22 3.33 22H12.67C13.41 22 14 21.41 14 20.67V5.33C14 4.6 13.4 4 12.67 4M11 16H5V19H11V16M11 11.5H5V14.5H11V11.5M23 10H20V3L15 13H18V21" /></svg>';
const noBattery = '<svg viewBox="0 0 24 24"><path d="M5,2V4H3.33A1.33,1.33 0 0,0 2,5.33V20.67C2,21.4 2.6,22 3.33,22H12.67C13.4,22 14,21.4 14,20.67V5.33A1.33,1.33 0 0,0 12.67,4H11V2H5M19,8V11.79L16.71,9.5L16,10.21L18.79,13L16,15.79L16.71,16.5L19,14.21V18H19.5L22.35,15.14L20.21,13L22.35,10.85L19.5,8H19M20,9.91L20.94,10.85L20,11.79V9.91M20,14.21L20.94,15.14L20,16.08V14.21Z" /></svg>';

$('name-logo').innerHTML = 'ESP32 BLE HUB';

function addConnectionRecord() {
  show('loader');
  const queryParams = new URLSearchParams({
    ble_id:  $('ble_id').value,
    targets:  $('targets').value.replace(', ',''),
  });

  fetch(`/addConnection?${queryParams}`)
  .then(response => response.text())
  .then(text => {
     openModal('Insert or update DB record', text);
     hide('loader');
  });
}

function addNewDevice() {
  show('loader');
  $('add-all').classList.add('hide');
  const queryParams = new URLSearchParams({
    ble_id:  $('ble_id').value,
    connected_to:  $('connected_to').value,
    ble_address:  $('ble_address').value,
    ble_name: $('ble_name').value
  });

  fetch(`/addDevice?${queryParams}`)
  .then(response => response.text())
  .then(text => {
    openModal('Insert or update DB record', text);
    hide('loader');
    const sel = document.querySelector('#ble-list .selected');
    if (sel) sel.classList.remove('gray');
  });
}


function addAllDevice() {
  show('loader');
  fetch('/addAllBle')
  .then(response => response.text())
  .then(text => {
     openModal('Insert or update DB record', text);
     $('add-all').classList.add('hide');
     hide('loader');
  });
}

function selectBle(row) {
  try {
    var rows = document.querySelectorAll('.ble-row');
    rows.forEach(row => {
      row.classList.remove('selected');
    });
    $(row.target.parentNode.id).classList.add('selected');
    $('add-ble').classList.remove('hide');
    let el = this.cells[0].querySelector('i');
    if (el) {
      $('ble_name').value = el.innerHTML;
    }
    else {
      $('ble_name').value = this.cells[0].innerHTML;
    }
    $('ble_id').value = this.cells[1].innerHTML;
    $('ble_address').value = this.cells[2].innerHTML;
    $('connected_to').value = this.cells[6].innerHTML;
    $('targets').value = this.cells[7].innerHTML;
    $('add-ble').innerHTML = this.cells[8].innerHTML != 'undefined' ? "Update Device" : "Add Device";
    $('add-connection').innerHTML = $('targets').value != "" ? "Update Connection" : "Add Connection";
    $('ble-sel').classList.add('flex-ctn');
    $('ble_id').focus();
    
  }
  catch(err) {console.log('error', err);}
}

function listBleDevices(){
  show('loader');
  fetch('/listble')
  .then(response => response.json())
  .then(obj => {
    $('ble-table').classList.remove('hide');
    let list = $('ble-list');
    list.innerHTML = "";

    // sort ble_devices
    let devArray = obj.ble_devices;
    devArray.sort((a, b) => {
      if (a.id !== b.id) {
        return a.id.localeCompare(b.id);
      }
      else {
        return a.address.localeCompare(b.address);
      }
    });

    devArray.forEach((item, i) => {
      if (item.name == null) item.name = '';
      if (item.targets == null) item.targets = '';
      var desc = item.name;
      if (desc == null || desc === '' ) {
        // desc = `<i style="color: gray;">${item.id.split(' ')[0] }_${item.address}</i>`;
        desc = `${item.device_name}`;
        show('nodb');
        console.log(desc);
      }

      // Create a single row with all columns
      var row = newEl('tr');
      var id = 'ble-' + i;
      row.setAttribute('id', id);
      row.classList.add('ble-row');
      row.innerHTML += `<td>${desc}</td>`;
      row.innerHTML += `<td>${item.id}</td>`;
      row.innerHTML += `<td>${item.address}</td>`;
      row.innerHTML += `<td class=desktop>${item.rssi} dBm</td>`;
      
      if (item.battery === 255) 
        row.innerHTML += `<td><span class=inline>USB ${noBattery}</span></td>`;
      else if (item.battery > 100) 
        row.innerHTML += `<td><span class=inline>${item.battery - 100}% ${charging}</span></td>`;
      else
        row.innerHTML += `<td><span class=inline>${item.battery}% ${noBattery}</span></td>`;
      
      if (item.connected) {
        row.innerHTML += `<td class=desktop><span class=inline>Yes ${bleYes}</span></td>`;
        row.innerHTML += `<td class=hide>${obj.hubName}</td>`;
      }
      else {
        row.innerHTML += `<td class=desktop><span class=inline>No ${bleNo}</span></td>`;
        row.innerHTML += '<td class=hide></td>' ;
      }
      row.innerHTML += `<td class=hide>${item.targets}</td>`;
      row.innerHTML += `<td class=hide>${item.db_present}</td>`;
      row.addEventListener('click', selectBle);
      
      if (!item.db_present)
        row.classList.add('gray');
        
      list.appendChild(row);
    });

    if (typeof obj.devices === 'object' && obj.devices !== null) {
      let devices = $('devices');
      let targetOptions = $('options');
      devices.innerHTML = "";
      targetOptions.innerHTML = "";
      obj.devices.forEach((item) => {
        // Fill devices datalist element
        let opt = newEl('option');
        opt.setAttribute('value', item);
        devices.appendChild(opt);
        // Fill target options element
        let t_opt = newEl('div');
        t_opt.setAttribute('data-value', item);
        t_opt.classList.add('option');
        t_opt.innerHTML = item;
        targetOptions.appendChild(t_opt);
      });
    }

    if (typeof obj.hubs === 'object' && obj.hubs !== null) {
      // Fill hubs datalist element
      let hubs = $('hubs');
      hubs.innerHTML = "";
      obj.hubs.forEach((item) => {
        let opt = newEl('option');
        opt.setAttribute('value', item);
        hubs.appendChild(opt);
      });
    }

    $('add-all').classList.remove('hide');
    hide('loader');
  });
}

// Expand or collapse the content according to current state
function expandCollapse() {
  // Get the HTML element immediately following the button (content)
  var content = this.nextElementSibling;
  if (content.style.maxHeight) {
    content.style.maxHeight = null;
    $('add-connection').classList.add('hide');
  }
  else {
    content.style.maxHeight = content.scrollHeight + "px";
    $('add-connection').classList.remove('hide');
  }
}

setTimeout(() => {
  $('list-ble').addEventListener('click', listBleDevices);
  $('add-ble').addEventListener('click', addNewDevice);
  $('add-connection').addEventListener('click', addConnectionRecord);
  $('collapsible').addEventListener('click', expandCollapse);
  $('add-all').addEventListener('click', addAllDevice);


  $('selectTrigger').addEventListener('click', () => {
    $('options').classList.remove('hide');
  });

  document.addEventListener('click', (event) => {
    if (!$('customSelect').contains(event.target)) {
      $('options').classList.add('hide');
    }
  });

  $('options').addEventListener('click', (event) => {
    if (event.target.classList.contains('option')) {
      $('targets').value += event.target.dataset.value + ',';
      $('options').classList.add('hide');
    }
  });
}, 500);
)string_literal";


static const char subtitle_js[] PROGMEM = R"string_literal(
  $('name-logo').remove();
  let el = newEl('div');
  el.classList.add('custom-header');
  el.innerHTML = '<h1>ESP32 BLE HUB</h1>' + '<span>' + options['subtitle-hidden'] + '</span><br>';
  document.querySelector('.title').appendChild(el);
)string_literal";

static const char mysqlconsole_html[] PROGMEM = R"string_literal(
<style>
pre {color: white;}
textarea {resize: vertical;}
#ws-log {text-align: left; max-height: 600px; overflow: auto; padding: 5px;}
.ws-box {font-family: monospace; font-size: smaller;width: 100%; background-color: black;}
.ws-output {color: white; }
#my-form, #input-text {width: 100%};
</style>

<form id="my-form">
  <textarea id="input-text" name="input-text" rows="4">SELECT * FROM ble_devices</textarea><br>
</form>
<div class="btn-bar" style="width:100%">
  <a id=send-sql class='btn ble'><span>Send SQL script</span></a>
</div>
<div class=ws-box>
  <pre id="ws-log"></pre>
</div>
)string_literal";


static const char mysqlconsole_js[] PROGMEM = R"string_literal(
function setupWebSocket() {
    const socket = new WebSocket('ws://' + location.hostname + ':81/');
    socket.addEventListener('message', function (event) {
         $('ws-log').innerHTML += event.data;
    });

    socket.addEventListener('open', function (event) {
        console.log('WebSocket connection opened:', event);
    });
    socket.addEventListener('close', function (event) {
        console.log('WebSocket connection closed:', event);
    });
    socket.addEventListener('error', function (event) {
        console.error('WebSocket error:', event);
    });
}

function sendSqlScript() {
  $('ws-log').innerHTML = '';
  var formData = new FormData();
  formData.append("sql", $('input-text').value);

  // POST data using the Fetch API
  fetch('/executeSql', {
    method: 'POST',
    body: formData
  })
  // Handle the server response
  .then(response => response.text())
  .then(text => {
    console.log(text);
  });
}


setTimeout(() => {
  setupWebSocket();
  $('send-sql').addEventListener('click', sendSqlScript);
}, 1000);
)string_literal";
