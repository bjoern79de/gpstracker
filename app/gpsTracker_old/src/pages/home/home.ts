import { Component } from '@angular/core';
import { NavController } from 'ionic-angular';
import { GoogleMaps, GoogleMap, GoogleMapsEvent, LatLng } from '@ionic-native/google-maps';
import { BLE } from '@ionic-native/ble';

@Component({
  selector: 'page-home',
  templateUrl: 'home.html'
})
export class HomePage {
  map: GoogleMap;
  private location:LatLng;
  marker: any;

  constructor(public navCtrl: NavController, private ble: BLE) {
    /*
    this.location = new LatLng(42.346903, -71.135101);
    this.map = GoogleMaps.create('map_canvas');
    this.marker = this.map.addMarkerSync({
      title: 'Ionic',
      icon: 'blue',
      animation: 'DROP',
      position: {
        lat: 43.0741904,
        lng: -89.3809802
      }
    });
    */
  }

  onDeviceDisconnected(peripheral) {
    console.log("disconnected");
  }

  onConnected(peripheral) {
    console.log(JSON.stringify(peripheral, null, 2));

    this.ble.read(peripheral.id, "00ff", "ff01").then(
      data => this.onValue(data),
      (err) => console.log(err)
    );

    this.ble.startNotification(peripheral.id, "00ff", "ff01").subscribe(
      data => this.onValue(data),
      (err) => console.log(err)
    );
    
  }

  onValue(buffer) {
    console.log(buffer.length);
  }

  onDeviceDiscovered(device) {
    console.log('Discovered ' + JSON.stringify(device, null, 2));
    //this.ble.autoConnect("98:7B:F3:65:DB:E8", this.onConnected, this.onDeviceDisconnected);
  }


  // If location permission is denied, you'll end up here
  scanError(error) {
    console.log(JSON.stringify(error, null, 2));
  }

  ionViewDidLoad() {
    //this.loadMap();
    //this.ble.autoConnect("98:7B:F3:65:DB:E8", this.onConnected, this.onDeviceDisconnected);

    this.ble.connect("24:0A:C4:07:86:92").subscribe(
      peripheral => this.onConnected(peripheral),
      peripheral => console.log('Disconnected', 'The peripheral unexpectedly disconnected')
    );

/*
    this.ble.scan([], 5).subscribe(
      device => this.onDeviceDiscovered(device), 
      error => this.scanError(error)
    );
*/
    let that = this;
/*
    setInterval(() => {
      console.log(0);
      (<any>window).plugins.gpsPlugin.getData(function(data) {
        console.log("1");
        that.location = new LatLng(data.lat, data.lng);
        let options = {
          target: that.location,
          zoom: 8
        };
        that.map.moveCamera(options);
        that.marker.setPosition(that.location);
        
        console.log("2");
      }, function() {})
    }, 1000);
    */
  }

  loadMap() {
    this.map.one(GoogleMapsEvent.MAP_READY).then(() => {
      let options = {
        target: this.location,
        zoom: 8
      };
      this.map.moveCamera(options);
    });
    this.map.addMarker(this.marker);
  }
}


