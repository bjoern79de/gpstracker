import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_blue/flutter_blue.dart';
import 'package:google_maps_flutter/google_maps_flutter.dart';
import 'widgets.dart';
import 'dart:io';
import 'dart:typed_data';
import 'package:geolocator/geolocator.dart';

void main() => runApp(new FlutterBlueApp());

class FlutterBlueApp extends StatefulWidget {
  FlutterBlueApp({Key key, this.title}) : super(key: key);

  final String title;

  @override
  _FlutterBlueAppState createState() => new _FlutterBlueAppState();
}

class _FlutterBlueAppState extends State<FlutterBlueApp> {
  FlutterBlue _flutterBlue = FlutterBlue.instance;

  GoogleMapController mapController;
  double lat = 0, lon = 0;

  /// Scanning
  StreamSubscription _scanSubscription;
  Map<DeviceIdentifier, ScanResult> scanResults = new Map();
  bool isScanning = false;

  /// State
  StreamSubscription _stateSubscription;
  BluetoothState state = BluetoothState.unknown;

  /// Device
  BluetoothDevice device;
  bool get isConnected => (device != null);
  StreamSubscription deviceConnection;
  StreamSubscription deviceStateSubscription;
  List<BluetoothService> services = new List();
  Map<Guid, StreamSubscription> valueChangedSubscriptions = {};
  BluetoothDeviceState deviceState = BluetoothDeviceState.disconnected;

  @override
  void initState() {
    super.initState();
    // Immediately get the state of FlutterBlue
    _flutterBlue.state.then((s) {
      setState(() {
        state = s;
      });
    });
    // Subscribe to state changes
    _stateSubscription = _flutterBlue.onStateChanged().listen((s) {
      setState(() {
        state = s;
      });
    });
  }

  @override
  void dispose() {
    _stateSubscription?.cancel();
    _stateSubscription = null;
    _scanSubscription?.cancel();
    _scanSubscription = null;
    deviceConnection?.cancel();
    deviceConnection = null;
    super.dispose();
  }

  _startScan() {
    _scanSubscription = _flutterBlue
        .scan(
      timeout: const Duration(seconds: 5),
      /*withServices: [
          new Guid('0000180F-0000-1000-8000-00805F9B34FB')
        ]*/
    )
        .listen((scanResult) {
      print('localName: ${scanResult.advertisementData.localName}');
      print(
          'manufacturerData: ${scanResult.advertisementData.manufacturerData}');
      print('serviceData: ${scanResult.advertisementData.serviceData}');
      setState(() {
        scanResults[scanResult.device.id] = scanResult;
      });
      if (scanResult.device.name == "UART Service") {
        _connect(scanResult.device);
        _stopScan();
      }
    }, onDone: _stopScan);

    setState(() {
      isScanning = true;
    });
  }

  _stopScan() {
    _scanSubscription?.cancel();
    _scanSubscription = null;
    setState(() {
      isScanning = false;
    });
  }

  _connect(BluetoothDevice d) async {
    device = d;
    // Connect to device
    deviceConnection = _flutterBlue
        .connect(device, timeout: const Duration(seconds: 10))
        .listen(
      null,
      onDone: _disconnect,
    );

    // Update the connection state immediately
    device.state.then((s) {
      setState(() {
        deviceState = s;
      });
    });

    // Subscribe to connection changes
    deviceStateSubscription = device.onStateChanged().listen((s) {
      setState(() {
        deviceState = s;
      });

      if (s == BluetoothDeviceState.connected) {
        device.discoverServices().then((s) {
          setState(() {
            services = s;

            services.forEach((service) async {
              if (service.uuid.toString() == "6e400001-b5a3-f393-e0a9-e50e24dcca9e") {
                for (BluetoothCharacteristic characteristic in service.characteristics) {
                  //List<int> value = await device.readCharacteristic(characteristic);
                  //print(value);
                  await _setNotification(characteristic);
                  sleep(const Duration(milliseconds:100));
                }
              }
            });
          });
        });
      }
    });
  }

  _disconnect() {
    // Remove all value changed listeners
    valueChangedSubscriptions.forEach((uuid, sub) => sub.cancel());
    valueChangedSubscriptions.clear();
    deviceStateSubscription?.cancel();
    deviceStateSubscription = null;
    deviceConnection?.cancel();
    deviceConnection = null;
    setState(() {
      device = null;
    });
  }

  _readCharacteristic(BluetoothCharacteristic c) async {
    await device.readCharacteristic(c);
    setState(() {});
  }

  _writeCharacteristic(BluetoothCharacteristic c) async {
    await device.writeCharacteristic(c, [0x12, 0x34],
        type: CharacteristicWriteType.withResponse);
    setState(() {});
  }

  _readDescriptor(BluetoothDescriptor d) async {
    await device.readDescriptor(d);
    setState(() {});
  }

  _writeDescriptor(BluetoothDescriptor d) async {
    await device.writeDescriptor(d, [0x12, 0x34]);
    setState(() {});
  }

  _setNotification(BluetoothCharacteristic c) async {
    if (c.isNotifying) {
      await device.setNotifyValue(c, false);
      // Cancel subscription
      valueChangedSubscriptions[c.uuid]?.cancel();
      valueChangedSubscriptions.remove(c.uuid);
    } else {
      await device.setNotifyValue(c, true);
      // ignore: cancel_subscriptions
      final sub = device.onValueChanged(c).listen((d) {
        setState(() {

          if (c.uuid.toString() == "6e400004-b5a3-f393-e0a9-e50e24dcca9e") {
            Uint8List data = d;
            lat = ((data[3] << 24) + (data[2] << 16) + (data[1] << 8) + (data[0] << 0)) / 10000000;
            print("lat: $lat");
            mapController.moveCamera(CameraUpdate.newLatLng(LatLng(lat, lon)));
            mapController.updateMarker(mapController.markers.elementAt(0), MarkerOptions(
                position: LatLng(lat, lon),
            ));
          }

          if (c.uuid.toString() == "6e400005-b5a3-f393-e0a9-e50e24dcca9e") {
            Uint8List data = d;
            lon = ((data[3] << 24) + (data[2] << 16) + (data[1] << 8) + (data[0] << 0)) / 10000000;
            mapController.moveCamera(CameraUpdate.newLatLng(LatLng(lat, lon)));

            mapController.updateMarker(mapController.markers.elementAt(0), MarkerOptions(
                position: LatLng(lat, lon),
            ));
          }

          print('onValueChanged $d');
        });
      });
      // Add to map
      valueChangedSubscriptions[c.uuid] = sub;
    }
    setState(() {});
  }

  _refreshDeviceState(BluetoothDevice d) async {
    var state = await d.state;
    setState(() {
      deviceState = state;
      print('State refreshed: $deviceState');
    });
  }

  _buildScanningButton() {
    if (isConnected || state != BluetoothState.on) {
      return null;
    }
    if (isScanning) {
      return new FloatingActionButton(
        child: new Icon(Icons.stop),
        onPressed: _stopScan,
        backgroundColor: Colors.red,
      );
    } else {
      return new FloatingActionButton(
          child: new Icon(Icons.search), onPressed: _startScan);
    }
  }

  _buildScanResultTiles() {
    return scanResults.values
        .map((r) => ScanResultTile(
      result: r,
      onTap: () => _connect(r.device),
    ))
        .toList();
  }

  List<Widget> _buildServiceTiles() {
    return services
        .map(
          (s) => new ServiceTile(
        service: s,
        characteristicTiles: s.characteristics
            .map(
              (c) => new CharacteristicTile(
            characteristic: c,
            onReadPressed: () => _readCharacteristic(c),
            onWritePressed: () => _writeCharacteristic(c),
            onNotificationPressed: () => _setNotification(c),
            descriptorTiles: c.descriptors
                .map(
                  (d) => new DescriptorTile(
                descriptor: d,
                onReadPressed: () => _readDescriptor(d),
                onWritePressed: () =>
                    _writeDescriptor(d),
              ),
            )
                .toList(),
          ),
        )
            .toList(),
      ),
    )
        .toList();
  }

  _buildActionButtons() {
    if (isConnected) {
      return <Widget>[
        new IconButton(
          icon: const Icon(Icons.cancel),
          onPressed: () => _disconnect(),
        )
      ];
    }
  }

  _buildAlertTile() {
    return new Container(
      color: Colors.redAccent,
      child: new ListTile(
        title: new Text(
          'Bluetooth adapter is ${state.toString().substring(15)}',
          style: Theme.of(context).primaryTextTheme.subhead,
        ),
        trailing: new Icon(
          Icons.error,
          color: Theme.of(context).primaryTextTheme.subhead.color,
        ),
      ),
    );
  }

  _buildDeviceStateTile() {
    return new ListTile(
        leading: (deviceState == BluetoothDeviceState.connected)
            ? const Icon(Icons.bluetooth_connected)
            : const Icon(Icons.bluetooth_disabled),
        title: new Text('Device is ${deviceState.toString().split('.')[1]}.'),
        subtitle: new Text('${device.id}'),
        trailing: new IconButton(
          icon: const Icon(Icons.refresh),
          onPressed: () => _refreshDeviceState(device),
          color: Theme.of(context).iconTheme.color.withOpacity(0.5),
        ));
  }

  _buildProgressBarTile() {
    return new LinearProgressIndicator();
  }

  // This widget is the root of your application.
  @override
  Widget build(BuildContext context) {
    var tiles = new List<Widget>();
    if (state != BluetoothState.on) {
      tiles.add(_buildAlertTile());
    }
    if (isConnected) {
      tiles.add(_buildDeviceStateTile());
      tiles.addAll(_buildServiceTiles());
    } else {
      tiles.addAll(_buildScanResultTiles());
    }

    return new MaterialApp(
      home: new Scaffold(
        appBar: new AppBar(
          title: const Text('Dog Tracker'),
          actions: _buildActionButtons(),
        ),
        floatingActionButton: _buildScanningButton(),
        body: new Stack(
          children: <Widget>[
            SizedBox.expand(
              child: GoogleMap(
                  onMapCreated: (controller) {
                    mapController = controller;
                    mapController.addMarker(MarkerOptions(
                      position: LatLng(mapController.cameraPosition.target.latitude, mapController.cameraPosition.target.longitude),
                      icon: BitmapDescriptor.fromAsset("assets/lemon.png"),
                      visible: true
                    ));

                    mapController.addMarker(MarkerOptions(
                        position: LatLng(mapController.cameraPosition.target.latitude, mapController.cameraPosition.target.longitude),
                        icon: BitmapDescriptor.defaultMarker,
                        visible: true
                    ));

                    var geolocator = Geolocator();
                    var locationOptions = LocationOptions(accuracy: LocationAccuracy.high, distanceFilter: 10);

                    StreamSubscription<Position> positionStream = geolocator.getPositionStream(locationOptions).listen(
                            (Position position) {
                              mapController.updateMarker(mapController.markers.elementAt(1), MarkerOptions(
                                  position: LatLng(position.latitude, position.longitude)
                              ));
                        });
                  },
                  mapType: MapType.satellite,
                  initialCameraPosition: CameraPosition(target: LatLng(1, 1))
              ),
            ),
            /*
            (isScanning) ? _buildProgressBarTile() : new Container(),
            new ListView(
              children: tiles,
            )
            */
          ],
        ),
      ),
    );
  }
}