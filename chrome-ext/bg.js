var requestTimeout = 1000 * 20;  // 20 seconds
var delay = 1; // minutes

var State = {
    VACANT: {id: 1, icon: "accept.png", color: [0, 190, 0, 230]},
    LIGHTS_ON: {id: 2, icon: "lightbulb.png", color: [0, 190, 0, 230]},
    OCCUPIED: {id: 3, icon: "stop.png", color: [208, 0, 24, 255]},
    OFFLINE: {id: 4, icon: "status_offline.png", color: [0, 190, 0, 230]}
};

var NotifyOptions = {
  type: "basic",
  title: "Vacant",
  message: "Vacant now!!",
  iconUrl: "logo.png"
};

function getLEUrl() {
    return "http://le.ap.int/state";
}

function updateIcon(state) {
    chrome.storage.local.get(["previousState", "stateChanged", "showNotify"], function (values) {
        var previousState = values["previousState"];
        var stateChanged = values["stateChanged"];
        var showNotify = values["showNotify"];

        if (previousState != state.id) {
            previousState = state.id;
            stateChanged = new Date().getTime();

            if(state == State.VACANT && showNotify){
                showVacantMessage();
                showNotify = false;
            }

            chrome.storage.local.set({
                "previousState": previousState,
                "stateChanged": stateChanged,
                "showNotify" : showNotify
            });
        }

        var stateDurationText = formatStateDuration(stateChanged);
        chrome.browserAction.setBadgeText({text: "" + stateDurationText});
        chrome.browserAction.setIcon({path: state.icon});
        chrome.browserAction.setBadgeBackgroundColor({color: state.color});

    });
}

function formatStateDuration(stateChanged) {
    var stateDuration = new Date(new Date() - new Date(stateChanged));
    if (stateDuration.getMinutes() > 0) {
        return stateDuration.getMinutes() + "m";
    }
    return stateDuration.getSeconds() + "s";
}

function scheduleRequest() {
    console.log('scheduleRequest');
    console.log('Scheduling for: ' + delay);
    console.log('Creating alarm');
    // Use a repeating alarm so that it fires again if there was a problem
    // setting the next alarm.
    chrome.alarms.create('refresh', {periodInMinutes: delay});
}

// ajax stuff
function startRequest(params) {
    // Schedule request immediately. We want to be sure to reschedule, even in the
    // case where the extension process shuts down while this request is
    // outstanding.
    if (params && params.scheduleRequest) scheduleRequest();

    updateState(
        function (vacant, lightsOn) {
            var state = State.OFFLINE;
            if (vacant && lightsOn) {
                state = State.LIGHTS_ON;
            } else {
                state = vacant ? State.VACANT : State.OCCUPIED;
            }

            updateIcon(state);
        },
        function () {
            updateIcon(State.OFFLINE);
        }
    );
}

function updateState(onSuccess, onError) {
    var xhr = new XMLHttpRequest();
    var abortTimerId = window.setTimeout(function () {
        xhr.abort();  // synchronously calls onreadystatechange
    }, requestTimeout);

    function handleSuccess(vacant, lightsOn) {
        window.clearTimeout(abortTimerId);
        if (onSuccess)
            onSuccess(vacant, lightsOn);
    }

    var invokedErrorCallback = false;

    function handleError() {
        window.clearTimeout(abortTimerId);
        if (onError && !invokedErrorCallback)
            onError();
        invokedErrorCallback = true;
    }

    try {
        xhr.onreadystatechange = function () {
            if (xhr.readyState != 4)
                return;

            if (xhr.responseText) {
                var jsonState = JSON.parse(xhr.responseText)
                handleSuccess(jsonState.state == "vacant", jsonState.lights == "on");
                return;
            }

            handleError();
        };

        xhr.onerror = function (error) {
            handleError();
        };

        xhr.open("GET", getLEUrl(), true);
        xhr.send(null);
    } catch (e) {
        handleError();
    }
}

function onInit() {
    console.log('onInit');
    startRequest({scheduleRequest: true});
    chrome.storage.local.set({
        "previousState": State.OFFLINE.id,
        "stateChanged": new Date().getTime()
    });

    chrome.alarms.create('watchdog', {periodInMinutes: 5});
}

function onAlarm(alarm) {
    console.log('Got alarm', alarm);
    if (alarm && alarm.name == 'watchdog') {
        onWatchdog();
    } else {
        startRequest({scheduleRequest: true});
    }
}

function onWatchdog() {
    chrome.alarms.get('refresh', function (alarm) {
        if (alarm) {
            console.log('Refresh alarm exists. Yay.');
        } else {
            console.log('Refresh alarm doesn\'t exist!? ' +
                'Refreshing now and rescheduling.');
            startRequest({scheduleRequest: true, showLoadingAnimation: false});
        }
    });
}

function onClick() {
    console.log('Manual update');
    startRequest({scheduleRequest: false});
    chrome.storage.local.set({
        "showNotify" : true
    });
}

function showVacantMessage(){
    console.log('Show Vacant Message');
	chrome.notifications.create(null, NotifyOptions, null);
}

function onStartUp() {
    console.log('Starting browser... updating icon.');
    startRequest({scheduleRequest: false});
    updateIcon(false);
}

chrome.runtime.onInstalled.addListener(onInit);
chrome.alarms.onAlarm.addListener(onAlarm);
chrome.browserAction.onClicked.addListener(onClick);
chrome.runtime.onStartup.addListener(onStartUp);
