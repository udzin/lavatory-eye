// Saves options to chrome.storage
function save_options() {
  var enableNotifications = document.getElementById('notifyBox').checked;
  chrome.storage.local.set({
    enableNotifications: enableNotifications
  }, function() {
    // Update status to let user know options were saved.
    var status = document.getElementById('status');
    status.textContent = 'Options saved (' +  enableNotifications + ').';
    setTimeout(function() {
      status.textContent = '';
    }, 750);
  });
}

// Restores select box and checkbox state using the preferences
// stored in chrome.storage.
function restore_options() {
  // Use default value color = 'red' and likesColor = true.
  chrome.storage.local.get({
    enableNotifications: true
  }, function(items) {
    document.getElementById('notifyBox').checked = items.enableNotifications;
  });
}
document.addEventListener('DOMContentLoaded', restore_options);
document.getElementById('save').addEventListener('click', save_options);