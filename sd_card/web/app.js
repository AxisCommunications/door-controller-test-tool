
/*************************************************************************
*  PACSIS ANGULAR APP
*  
*************************************************************************/
var app = angular.module('app', ['ui.bootstrap', 'ngRoute'])

app.config(['$routeProvider',
  function($routeProvider) {
    $routeProvider.
      when('/', {
        templateUrl: 'doorctrl.htm',
        //controller: 'AddOrderController'
      }).
      when('/config', {
        templateUrl: 'config.htm',
        //controller: 'ShowOrdersController'
      }).
      otherwise({
        redirectTo: '/'
      });
  }]);


/*************************************************************************
*  ANGULAR SERVICES
*  
*************************************************************************/

/**
* Service to get the currect door configuration.
*/     
app.factory('ConfigService', function($http, $location) {
  var doors = [];
  var arduinoUrl = 'http://' + $location.host();
  if (!($location.port() == 80)) {
    arduinoUrl += ":" + $location.port();
   }

  return {
    getDoorConfig: function(successCallback, errorCallback) {        
      return $http.get(arduinoUrl + '/api?cmd=getdoorconfig', {timeout: 5000})
      .then(successCallback, errorCallback);            
    }
  }
});

/**
* ẀEBSOCKET SERVICE
* Wraps websocket functionality.
*/
app.factory('WebsocketService', function($http, $location) {
  var service = {};

  service.connect = function() {
    if(service.ws) { 
      service.disconnect();
      console.log("Reconnecting.");
    }
    else {
      console.log("Connecting.");
    }

    var ws = new WebSocket("ws://"+ $location.host()+":8888");

    ws.onopen = function() {
      console.log("Websocket connection established. Requesting update.");
      service.requestUpdate();
    };

    ws.onclose = function() {
      console.log("Websocket connection closed.");
    };

    ws.onerror = function() {
      console.log("Failed to establish a websocket connection.");
    };

    ws.onmessage = function(message) {
      if (service.updateCallback) {
        service.updateCallback(JSON.parse(message.data));
      }
    };

    service.ws = ws;
  }

  service.disconnect = function() {
    if(service.ws) {
      service.ws.close();
      service.ws = null;
    }
  }

  service.subscribeToUpdates = function(callback) {
    service.updateCallback = callback;
  }

  service.isConnected = function() {
    if (service.ws) {
      return true;
    } 
    else {
      return false;
    }
  }

  //
  // User commands sent from GUI.
  //
  service.pressKeypad = function(digit, doorId, readerId) {    
    service.ws.send(JSON.stringify({
      "EnterPIN": {
        "DoorId": doorId,
        "Id": readerId,
        "PIN": digit
      }
    }));
  }

  service.sendPinSequence = function(pinSequence, doorId, readerId) {    
    service.ws.send(JSON.stringify({
      "EnterPIN": {
        "DoorId": doorId,
        "Id": readerId,
        "PIN": pinSequence
      }
    }));
  }      

  service.swipeCard = function(facilityCode, cardNumber, doorId, readerId) {    
    service.ws.send(JSON.stringify({
      "SwipeCard": {
        "DoorId": doorId,
        "Id": readerId,
        "FacilityCode": facilityCode,
        "CardNumber": cardNumber
      }
    }));
  }      

  service.pushRexButton = function(doorId, id) {    
    service.ws.send(JSON.stringify({
      "PushREX": {
        "DoorId": doorId,
        "Id": id
      }   
    }));
  }      

  service.openDoor = function(doorId, id) {    
    service.ws.send(JSON.stringify({
      "OpenDoor": {
        "DoorId": doorId,
        "Id": id
      }   
    }));
  }      

  service.closeDoor = function(doorId, id) {    
    service.ws.send(JSON.stringify({
      "CloseDoor": {
        "DoorId": doorId,
        "Id": id
      }   
    }));
  }

  service.activateInput = function(doorId, id) {    
    service.ws.send(JSON.stringify({
      "ActivateInput": {
        "DoorId": doorId,
        "Id": id
      }   
    }));
  }      

  service.deactivateInput = function(doorId, id) {    
    service.ws.send(JSON.stringify({
      "DeactivateInput": {
        "DoorId": doorId,
        "Id": id
      }   
    }));
  }      
  
  service.requestUpdate = function() {    
    service.ws.send(JSON.stringify({
      "RequestUpdate": {}   
    }));
  }  

  return service;

});

/**
* ALERT SERVICE
* Handles notifications.
*/
app.factory('AlertService', function() {
  alertService = {}
  alertService.alerts = [];
  
  alertService.addAlert = function(alertType, alertMessage) {
    alertService.alerts.push({type: alertType, msg: alertMessage});
  };

  alertService.closeAlert = function(index) {
    alertService.alerts.splice(index, 1);
  };

  return alertService;

});

/*************************************************************************
*
*  CONTROLLERS
*  
*************************************************************************/

app.controller('NavCtrl', ['$scope', '$location', 'WebsocketService',
  function($scope, $location, WebsocketService) {  

    $scope.location = $location

    $scope.connectionButtonText = function() {
      if (WebsocketService.isConnected())
        return "Disconnect";
      else
          return "Connect";
      }

    $scope.toggleConnection = function() {
      if (WebsocketService.isConnected()) {
        WebsocketService.disconnect();
      }
      else {
        WebsocketService.connect();
      }
    }

}]);

app.controller('AlertCtrl', ['$scope', 'AlertService', 
  function ($scope, AlertService) {
    $scope.alerts = AlertService.alerts;

    $scope.closeAlert = function(index) {
      AlertService.closeAlert(index);
    };
  }

]);

app.controller('DoorsCtrl', ['$scope', 'ConfigService', 'WebsocketService', 'AlertService',
  function($scope, ConfigService, WebsocketService, AlertService) {  
    
    var keypadPressSound = new Audio("keypad.mp3");

    $scope.isConnected = function() {
      return WebsocketService.isConnected();
    };
 

    $scope.keypadPress = function(digit, doorId, readerId) {
      if (digit && doorId && readerId) {        
        WebsocketService.pressKeypad(digit, doorId, readerId);
      }
    };

    $scope.keypadPlaySound = function () {
      keypadPressSound.currentTime = 0;
      keypadPressSound.play();
    }

    $scope.sendPinSequence = function(pinSequence, doorId, readerId) {
      if (pinSequence != "" && doorId != "" && readerId != "") {
        WebsocketService.sendPinSequence(pinSequence, doorId, readerId);
      }
    };

    $scope.swipeCard = function(facilityCode, cardNumber, doorId, readerId) {
      if (facilityCode && cardNumber && doorId && readerId) {
        WebsocketService.swipeCard(facilityCode, cardNumber, doorId, readerId);
      }
    };

    $scope.pushRexButton = function(doorId, id) {
      if (doorId && id) {
        WebsocketService.pushRexButton(doorId, id);
      }
    };

    $scope.openDoor = function(doorId, id) {
      if (doorId && id) {
        WebsocketService.openDoor(doorId, id);
      }
    };

    $scope.closeDoor = function(doorId, id) {
      if (doorId && id) {
        WebsocketService.closeDoor(doorId, id);
      }
    };
	
    $scope.activateInput = function(doorId, id) {
      if (doorId && id) {
        WebsocketService.activateInput(doorId, id);
      }
    };

    $scope.deactivateInput = function(doorId, id) {
      if (doorId && id) {
        WebsocketService.deactivateInput(doorId, id);
      }
    };

    function isActive(doorId, id) {      
      var door = findDoor($scope.doors, doorId);
      return findActiveState(door, id);
    }

    function findActiveState(obj, id) {
      for (var i in obj) {
        if (!obj.hasOwnProperty(i)) {
          continue;
        }
        if (typeof obj[i] == 'object' || obj[i] == 'array') {
            findActiveState(obj[i], id, isActive);
        } 
        else if (i == "Id" && obj["Id"] == id) {                
            return obj["IsActive"];
        }
      }
    }

    function findAndUpdateActiveState(obj, id, isActive) {
      for (var i in obj) {
        if (!obj.hasOwnProperty(i)) {
          continue;
        }
        if (typeof obj[i] == 'object' || typeof obj[i] == 'array') {
            findAndUpdateActiveState(obj[i], id, isActive);
        } 
        else if (i == "Id" && obj["Id"] == id) {                
            obj["IsActive"] = isActive;
        }
      }
    }

    function findById(object, id, callback) {
      for (var property in object) {
        if (object.hasOwnProperty(property)) 
        {
          if (typeof object[property] == "object" || typeof object[property] == "array"){
              findById(object[property], id, callback);
          }
          else
          {            
            if (property == "Id" && object[property] == id)
              callback(object);
          }
        }
      }
    }

    /*
    * Returns the Door open/close button text, depending on door monitor state
    * and state of connected lock(s). Door button text should reflect the status of the 
    * lock(s), i.e. Force Open when lock is active and Open when unlocked.
    */
    $scope.getDoorButton = function(door, doorMonitor) {    
      var button = {"text": "",
                    "action": null};
      
      var allLocksUnlocked = true;
      var allLocksHaveActiveStates = true;
      var doorOpen;
      var doorId = door["Id"];
      var doorMonitorId = doorMonitor["Id"];

      
      // First check that we know the current active state for the 
      // door monitor.
      if (!doorMonitor.hasOwnProperty("IsActive")) {        
        return button;
      }
      else {        
        doorOpen = doorMonitor["IsActive"]
      }

      // Now we ensure that each lock has an active state. If so we set the
      // door monitor text accordingly.
      door.Lock.forEach(function(lock) {
        if (!lock.hasOwnProperty("IsActive")) {
          allLocksHaveActiveStates = false;
          return false; // Break out of forEach loop.
        }
        if(lock.IsActive) {
          allLocksUnlocked = false;
          return false; // Break out of forEach loop.
        }   
      });

      if(!allLocksHaveActiveStates) {
        return button;
      }
      
      if (allLocksUnlocked && doorOpen) {
        button.text = "Close Door";
        button.action = function() {
          $scope.closeDoor(doorId, doorMonitorId);
        };
      } 
      else if (allLocksUnlocked && !doorOpen) {
        button.text = "Open Door";
        button.action = function() {
          $scope.openDoor(doorId, doorMonitorId);
        };

      }
      else if (!allLocksUnlocked && doorOpen) {
        button.text = "Force Close Door";
        button.action = function() {
          $scope.closeDoor(doorId, doorMonitorId);
        };

      }
      else if (!allLocksUnlocked && !doorOpen) {
        button.text = "Force Open Door";
        button.action = function() {
          $scope.openDoor(doorId, doorMonitorId);
        };

      }

      return button;
    }

    /*
    * Returns the Input activate/deactivate button text, depending on input state. 
	* Input button text should reflect the status of the contact, i.e. Activate when
	* deactivated and Deactivate when activated.
    */
    $scope.getInputButton = function(door, input) {    
      var button = {"text": "",
                    "action": null};
      
      var doorId = door["Id"];
      var inputId = input["Id"];
      
      // First check that we know the current active state for the input.
      if (!input.hasOwnProperty("IsActive")) {        
        return button;
      }
      else {        
		  if (input["IsActive"]) {
			button.text = "Deactivate";
			button.action = function() {
			  $scope.deactivateInput(doorId, inputId);
			};
		  } 
		  else {
			button.text = "Activate";
			button.action = function() {
			  $scope.activateInput(doorId, inputId);
			};
		  }
	  }
	  
      return button;
    }
	

    /*
    * Handle updates from the Arduino.
    */
    WebsocketService.subscribeToUpdates(function(updateMessage) {
      var doorId = updateMessage["Update"]["DoorId"];
      var id = updateMessage["Update"]["Id"];
      var isActive = updateMessage["Update"]["IsActive"];

      // First we find which door the update is referring to...
      findById($scope.doors, doorId, function(door) {        
        if (!door) {
          console.log("Door id not found: " + doorId);
          return;
        }
        // then we find the peripheral...
        findById(door, id, function(peripheral) {
          if (!peripheral) {
            console.log("Peripheral id not found: " + id);
            return;
          }            
          // and update its IsActive property!
          // It is wrapped in $apply for angular to know that it has been changed.
          $scope.$apply(function () {
            peripheral["IsActive"] = isActive;
          });
        });
      });
    });

    /*
    * Get the door and network config from the Arduino.
    */
    ConfigService.getDoorConfig(      
      // If request succeded we create the doors object.
      function(response) {
        if (response.status == 200) {
          $scope.doors = [];
          try {
            JSON.stringify(response.data)
          } catch (e) {
            AlertService.addAlert("danger", "Door config is not in correct JSON format: \"" + e + "\"");    
          }
          for (var key in response.data) {             
             $scope.doors.push(response.data[key]);
          }          
        }
      }, 
      // If request failed we show an alert.
      function(response) {
        AlertService.addAlert("danger", "Could not get configuration from Arduino.");
      }
    );

    // If user navigates away from webapp, we disconnec the websocket connection as 
    // it cannot be resumed.
    $scope.$on('$locationChangeStart', function (event, newLoc, oldLoc){
      WebsocketService.disconnect();
    });

}]);


/*************************************************************************
*
*  DIRECTIVES
*  
*************************************************************************/
app.directive('reader', function() {
  return {
    restrict: 'E',
    templateUrl: 'reader.htm'
  }
});

app.directive('rex', function() {
  return {
    restrict: 'E',
    templateUrl: 'rex.htm'
  }
});

app.directive('doormonitor', function() {
  return {
    restrict: 'E',
    templateUrl: 'doormon.htm'
  }
});

app.directive('lock', function() {
  return {
    restrict: 'E',
    templateUrl: 'lock.htm'
  }
});

app.directive('digitalinput', function() {
  return {
    restrict: 'E',
    templateUrl: 'input.htm'
  }
});

app.directive('digitaloutput', function() {
  return {
    restrict: 'E',
    templateUrl: 'output.htm'
  }
});



window.onbeforeunload = function(e) {
    var angularWSCtrlScope = angular.element($("#MySuperAwesomeApp")).scope();
    return;
};