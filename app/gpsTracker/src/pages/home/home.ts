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
  private location: LatLng;
  marker: any;
  peripheral: any;
  fixType: any;
  numSats: any;
  lat: any;
  lon: any;
  speed: any;
  height: any;

  constructor(public navCtrl: NavController, private ble: BLE) {
    this.lat = 0;
    this.lon = 0;
    this.numSats = 0;
    this.speed = 0;

    this.location = new LatLng(42.346903, -71.135101);
    this.map = GoogleMaps.create('map_canvas');
    this.map.setMapTypeId("MAP_TYPE_HYBRID");
    this.marker = this.map.addMarkerSync({
      title: 'Ionic',
      icon: 'blue',
      animation: 'DROP',
      position: {
        lat: 43.0741904,
        lng: -89.3809802
      }
    });
  }

  onDeviceDisconnected(peripheral) {
    console.log("disconnected");
  }

  onConnected(peripheral) {
    this.peripheral = peripheral;
    console.log(JSON.stringify(peripheral, null, 2));

    let that = this;

    this.ble.startNotification(peripheral.id, "6e400001-b5a3-f393-e0a9-e50e24dcca9e", "6e400002-b5a3-f393-e0a9-e50e24dcca9e").subscribe(
      buffer => {
        var data = new Int32Array(buffer);
        that.fixType = data[0];
      },
      (err) => console.log(err)
    );

    this.ble.startNotification(peripheral.id, "6e400001-b5a3-f393-e0a9-e50e24dcca9e", "6e400003-b5a3-f393-e0a9-e50e24dcca9e").subscribe(
      buffer => {
        var data = new Int32Array(buffer);
        that.numSats = data[0];
      },
      (err) => console.log(err)
    );
    
    this.ble.startNotification(peripheral.id, "6e400001-b5a3-f393-e0a9-e50e24dcca9e", "6e400004-b5a3-f393-e0a9-e50e24dcca9e").subscribe(
      buffer => {
        var data = new Int32Array(buffer);
        that.lat = data[0] / 10000000;
      },
      (err) => console.log(err)
    );

    this.ble.startNotification(peripheral.id, "6e400001-b5a3-f393-e0a9-e50e24dcca9e", "6e400005-b5a3-f393-e0a9-e50e24dcca9e").subscribe(
      buffer => {
        var data = new Int32Array(buffer);
        that.lon = data[0] / 10000000;
      },
      (err) => console.log(err)
    );

    this.ble.startNotification(peripheral.id, "6e400001-b5a3-f393-e0a9-e50e24dcca9e", "6e400006-b5a3-f393-e0a9-e50e24dcca9e").subscribe(
      buffer => {
        var data = new Int32Array(buffer);
        that.speed = data[0];
      },
      (err) => console.log(err)
    );

    this.ble.startNotification(peripheral.id, "6e400001-b5a3-f393-e0a9-e50e24dcca9e", "6e400007-b5a3-f393-e0a9-e50e24dcca9e").subscribe(
      buffer => {
        var data = new Int32Array(buffer);
        that.height = data[0];
      },
      (err) => console.log(err)
    );

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
    this.loadMap();
    this.ble.autoConnect("30:AE:A4:88:8E:46", this.onConnected.bind(this), this.onDeviceDisconnected.bind(this));
/*
    this.ble.scan([], 5).subscribe(
      device => this.onDeviceDiscovered(device), 
      error => this.scanError(error)
    );
*/

    let that = this;

    setInterval(() => {
        that.location = new LatLng(that.lat, that.lon);
        let options = {
          target: that.location
        };
        console.log(that.lat, that.lon);
        that.map.moveCamera(options);
        that.marker.setPosition(that.location);
    }, 1000);
  }

  ionViewWillLeave() {
    console.log('ionViewWillLeave disconnecting Bluetooth');
    this.ble.disconnect(this.peripheral.id).then(
      () => console.log('Disconnected ' + JSON.stringify(this.peripheral)),
      () => console.log('ERROR disconnecting ' + JSON.stringify(this.peripheral))
    )
  }

  loadMap() {
    this.map.one(GoogleMapsEvent.MAP_READY).then(() => {
      let options = {
        target: this.location,
        zoom: 15
      };
      this.map.moveCamera(options);
    });
    this.map.addMarker(this.marker);
  }
}


