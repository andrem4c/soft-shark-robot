import UIKit
import CoreBluetooth

class ViewController: UIViewController, CBCentralManagerDelegate, CBPeripheralDelegate {
    
    
    @IBOutlet weak var dataLabel: UILabel!
    @IBOutlet weak var statusLabel: UILabel!
    @IBOutlet weak var dutySlider: UISlider!
    @IBOutlet weak var dutyLabel: UILabel!
    
    @IBOutlet weak var temperatureLabel1: UILabel!
    @IBOutlet weak var statusLabel1: UILabel!
    @IBOutlet weak var temperatureLabel2: UILabel!
    @IBOutlet weak var statusLabel2: UILabel!
    @IBOutlet weak var restTimeSlider: UISlider!
    @IBOutlet weak var restTimeLabel: UILabel!
    @IBOutlet weak var timeButton: UIButton!
    
    var centralManager: CBCentralManager!
    var discoveredPeripheral: CBPeripheral?
    var dataCharacteristic: CBCharacteristic?
    var peripherals: [CBPeripheral] = []
    var writeCharacteristic: CBCharacteristic?
    var userInitiatedDisconnect = false

    
    // UUIDs services and characteristics defined in ESP32S3
    let serviceUUID = CBUUID(string: "12345678-1234-1234-1234-1234567890AB")
    let characteristicUUID = CBUUID(string: "ABCD1234-AB12-CD34-EF56-ABCDEF123456")
    let writeUUID = CBUUID(string: "FFF1FFF1-1111-2222-3333-444455556666")

    
    override func viewDidLoad() {
        super.viewDidLoad()
        centralManager = CBCentralManager(delegate: self, queue: nil)
        activeTime()
    }
    
    @IBAction func scanButton(_ sender: Any) {
        peripherals.removeAll()
        userInitiatedDisconnect = false
        if centralManager.state == .poweredOn {
            centralManager.scanForPeripherals(withServices: nil, options: nil)
            print("🔍 Scanning...")
            DispatchQueue.main.async {
                self.statusLabel.text = "🔍 Searching for devices..."
            }
            // Show the list after 3 seconds
            DispatchQueue.main.asyncAfter(deadline: .now() + 3.0) {
                self.centralManager.stopScan()
                self.showDeviceSelectionAlert()
            }
        }
    }

    
    func showDeviceSelectionAlert() {
        let alert = UIAlertController(title: "BLE devices found", message: nil, preferredStyle: .actionSheet)
        for peripheral in peripherals {
            let action = UIAlertAction(title: peripheral.name ?? "Unnamed", style: .default) { _ in
                self.discoveredPeripheral = peripheral
                print("📍 Selected: \(peripheral.name ?? "Unnamed")")
                DispatchQueue.main.async {
                    self.statusLabel.text = "📍 Device selected: \(peripheral.name ?? "Unnamed")"
                }
            }
            alert.addAction(action)
        }

        let cancel = UIAlertAction(title: "Cancel", style: .cancel, handler: nil)
        alert.addAction(cancel)

        if let popover = alert.popoverPresentationController {
            popover.sourceView = self.view
            popover.sourceRect = CGRect(x: self.view.bounds.midX, y: self.view.bounds.midY, width: 0, height: 0)
            popover.permittedArrowDirections = []
        }

        self.present(alert, animated: true, completion: nil)
    }


    @IBAction func connectButton(_ sender: Any) {
        if let peripheral = discoveredPeripheral {
            userInitiatedDisconnect = false
            centralManager.connect(peripheral, options: nil)
            print("🔗 Connecting to \(peripheral.name ?? "Unnamed")...")
            DispatchQueue.main.async {
                self.statusLabel.text = "🔗 Connecting to \(peripheral.name ?? "Unnamed")..."
            }
        } else {
            print("⚠️ Select a device before connecting.")
            DispatchQueue.main.async {
                self.statusLabel.text = "⚠️ Select a device before connecting."
            }
        }
    }


    @IBAction func disconnectButton(_ sender: Any) {
        if let peripheral = discoveredPeripheral {
            userInitiatedDisconnect = true
            centralManager.cancelPeripheralConnection(peripheral)
            print("🔌 Disconnected")
            DispatchQueue.main.async {
                self.statusLabel.text = "🔌 Disconnected"
                self.dataLabel.text = "Offline"
            }
        }
    }

    @IBAction func startButton(_ sender: Any) {
        sendCommand("START")
    }
    
    @IBAction func stopButton(_ sender: Any) {
        sendCommand("STOP")
    }
    
    @IBAction func forwardButton(_ sender: Any) {
        sendCommand("F")
    }
        
    @IBAction func leftButton(_ sender: Any) {
        sendCommand("L")
    }
    
    @IBAction func rightButton(_ sender: Any) {
        sendCommand("R")
    }
    
    @IBAction func sliderChanged(_ sender: UISlider) {
        let percent = Int(sender.value)
        dutyLabel.text = "\(percent)%"
            
        // Convert the percent (0–75%) to the duty cycle value (0–191)
        let dutyValue = Int(Float(percent) * (255.0 / 100.0))
        sendCommand("D:\(dutyValue)")
    }
    
    @IBAction func restSliderChanged(_ sender: UISlider) {
        let restTime = Int(sender.value)
            restTimeLabel.text = "\(restTime) ms"
            sendCommand("R:\(restTime)")
    }
    
    // MARK: - Bluetooth Delegates

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state != .poweredOn {
            print("⚠️ Bluetooth is not turned on")
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String : Any], rssi RSSI: NSNumber) {
        if let name = peripheral.name, !peripherals.contains(peripheral) {
                print("📡 Detected: \(name)")
                peripherals.append(peripheral)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        let name = peripheral.name ?? "ESP32"
        print("✅ Connected to \(name)")
        DispatchQueue.main.async {
            self.statusLabel.text = "✅ Connected to \(name)"
            self.dataLabel.text = "Ready for deployment!"
        }
        peripheral.delegate = self
        peripheral.discoverServices([serviceUUID])
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let services = peripheral.services {
            for service in services {
                print("🔧 Service: \(service.uuid)")
                let writeUUID = CBUUID(string: "FFF1FFF1-1111-2222-3333-444455556666")
                peripheral.discoverCharacteristics([characteristicUUID, writeUUID], for: service)

            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for characteristic in characteristics {
            print("Characteristic UUID: \(characteristic.uuid)")

            if characteristic.uuid == characteristicUUID {
                // Notification (ESP32 → iPhone)
                dataCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            } else if characteristic.uuid == writeUUID {
                // Writting (iPhone → ESP32)
                writeCharacteristic = characteristic
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value,
              let message = String(data: data, encoding: .utf8) else { return }
        
        print("Received: \(message)")

        DispatchQueue.main.async {
            if message.contains("T1:") {
                
                // Message with temperature
                let parts = message.components(separatedBy: ";")
                for part in parts {
                    if part.contains("T1:") {
                        let values = part.replacingOccurrences(of: "T1:", with: "").components(separatedBy: ",")
                        if values.count == 2 {
                            self.temperatureLabel1.text = values[0] + " °C"
                            self.statusLabel1.text = values[1]
                        }
                    } else if part.contains("T2:") {
                        let values = part.replacingOccurrences(of: "T2:", with: "").components(separatedBy: ",")
                        if values.count == 2 {
                            self.temperatureLabel2.text = values[0] + " °C"
                            self.statusLabel2.text = values[1]
                        }
                    }
                }
            } else {
                // General message (movment, status, etc.)
                self.dataLabel.text = message
            }
        }
    }

    func sendCommand(_ command: String) {
        guard let peripheral = discoveredPeripheral,
            let writeChar = writeCharacteristic else {
            print("⚠️ Not connected or characteristic is not available.")
            return
        }

        if let data = command.data(using: .utf8) {
            peripheral.writeValue(data, for: writeChar, type: .withResponse)
            print("📤 Command sent: \(command)")
        } else {
            print("❌ Error encoding the command.")
        }
    }
    
    func activeTime() {
        let options = [
            ("0.5 seconds", "500"),
            ("1 second", "1000"),
            ("1.5 seconds", "1500"),
            ("2 seconds", "2000"),
            ("3 seconds", "3000")

        ]
        
        let actions = options.map { (button_title, button_value) in
            UIAction(title: button_title, handler: { _ in
                self.timeButton.setTitle(button_title, for: .normal)
                self.sendCommand("T:\(button_value)")
            })
        }
        
        timeButton.menu = UIMenu(title: "Activation time", children: actions)
        timeButton.showsMenuAsPrimaryAction = true
    }

}

