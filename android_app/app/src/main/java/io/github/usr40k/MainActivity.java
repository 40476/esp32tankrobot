 package io.github.usr40k.esp32tankrobot;

 import android.Manifest;
 import android.annotation.SuppressLint;
 import android.bluetooth.BluetoothAdapter;
 import android.bluetooth.BluetoothDevice;
 import android.content.DialogInterface;
 import android.content.Intent;
 import android.content.pm.PackageManager;
 import android.os.AsyncTask;
 import android.os.Bundle;
 import android.os.Handler;
 import android.os.Looper;
 import android.view.View;
 import android.view.WindowManager;
 import android.widget.AdapterView;
 import android.widget.ArrayAdapter;
 import android.widget.Button;
 import android.widget.SeekBar;
 import android.widget.Spinner;
 import android.widget.TextView;
 import android.widget.Toast;

 import androidx.annotation.NonNull;
 import androidx.appcompat.app.AlertDialog;
 import androidx.appcompat.app.AppCompatActivity;
 import androidx.core.app.ActivityCompat;

 import java.util.ArrayList;
 import java.util.List;
 import java.util.Set;

 public class MainActivity extends AppCompatActivity {
     private static final int PERMISSION_REQUEST_CODE = 1001;
     private static final String[] REQUIRED_PERMISSIONS = {
             Manifest.permission.BLUETOOTH_CONNECT,
             Manifest.permission.BLUETOOTH_SCAN,
             Manifest.permission.ACCESS_FINE_LOCATION
     };

     private BluetoothAdapter bluetoothAdapter;
     private BluetoothService bluetoothService;
     private Handler uiHandler;

     // UI Components
     private TextView connectionStatus;
     private Button connectButton;
     private Button disconnectButton;

     // Tank controls
     private SeekBar leftTankSeekBar;
     private SeekBar rightTankSeekBar;
     private TextView leftTankValue;
     private TextView rightTankValue;
     private Button tankStopButton;

     // Arm controls
     private Spinner motorSpinner;
     private Button clawGrabForward;
     private Button clawGrabBackward;
     private Button clawRotateForward;
     private Button clawRotateBackward;
     private Button middleReachForward;
     private Button middleReachBackward;
     private Button baseReachForward;
     private Button baseReachBackward;
     private Button stopSelectedMotor;
     private Button stopAllButton;

     private int selectedMotor = 0;
     private boolean[] motorStates = new boolean[4]; // false=stopped, true=moving

     @Override
     protected void onCreate(Bundle savedInstanceState) {
         super.onCreate(savedInstanceState);
         getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
         setContentView(R.layout.activity_main);

         uiHandler = new Handler(Looper.getMainLooper());
         bluetoothService = new BluetoothService(uiHandler);
         bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

         initializeViews();
         setupClickListeners();
         checkPermissions();
     }

     private void initializeViews() {
         connectionStatus = findViewById(R.id.connectionStatus);
         connectButton = findViewById(R.id.connectButton);
         disconnectButton = findViewById(R.id.disconnectButton);

         leftTankSeekBar = findViewById(R.id.leftTankSeekBar);
         rightTankSeekBar = findViewById(R.id.rightTankSeekBar);
         leftTankValue = findViewById(R.id.leftTankValue);
         rightTankValue = findViewById(R.id.rightTankValue);
         tankStopButton = findViewById(R.id.tankStopButton);

         motorSpinner = findViewById(R.id.motorSpinner);
         clawGrabForward = findViewById(R.id.clawGrabForward);
         clawGrabBackward = findViewById(R.id.clawGrabBackward);
         clawRotateForward = findViewById(R.id.clawRotateForward);
         clawRotateBackward = findViewById(R.id.clawRotateBackward);
         middleReachForward = findViewById(R.id.middleReachForward);
         middleReachBackward = findViewById(R.id.middleReachBackward);
         baseReachForward = findViewById(R.id.baseReachForward);
         baseReachBackward = findViewById(R.id.baseReachBackward);
         stopSelectedMotor = findViewById(R.id.stopSelectedMotor);
         stopAllButton = findViewById(R.id.stopAllButton);

         // Setup tank seekbars
         leftTankSeekBar.setMax(400);
         leftTankSeekBar.setProgress(200);
         rightTankSeekBar.setMax(400);
         rightTankSeekBar.setProgress(200);

         updateTankValues();

         // Setup motor spinner
         String[] motors = {"Claw Grab (0)", "Claw Rotate (1)", "Middle Reach (2)", "Base Reach (3)"};
         ArrayAdapter<String> adapter = new ArrayAdapter<>(this,
                 android.R.layout.simple_spinner_item, motors);
         adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
         motorSpinner.setAdapter(adapter);

         motorSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
             @Override
             public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                 selectedMotor = position;
                 updateMotorButtons();
             }

             @Override
             public void onNothingSelected(AdapterView<?> parent) {}
         });

         bluetoothService.setConnectionListener(new BluetoothService.ConnectionListener() {
             @Override
             public void onConnected(String deviceName) {
                 runOnUiThread(() -> {
                     connectionStatus.setText("Connected: " + deviceName);
                     connectionStatus.setTextColor(getColor(R.color.green));
                     connectButton.setEnabled(false);
                     disconnectButton.setEnabled(true);
                 });
             }

             @Override
             public void onDisconnected() {
                 runOnUiThread(() -> {
                     connectionStatus.setText("Disconnected");
                     connectionStatus.setTextColor(getColor(R.color.red));
                     connectButton.setEnabled(true);
                     disconnectButton.setEnabled(false);
                 });
             }

             @Override
             public void onMessageReceived(String message) {
                 // Handle incoming messages from ESP32 if needed
                 runOnUiThread(() -> {
                     Toast.makeText(MainActivity.this,
                             "RX: " + message, Toast.LENGTH_SHORT).show();
                 });
             }

             @Override
             public void onError(String error) {
                 runOnUiThread(() -> {
                     connectionStatus.setText("Error: " + error);
                     connectionStatus.setTextColor(getColor(R.color.red));
                 });
             }
         });
     }

     private void setupClickListeners() {
         connectButton.setOnClickListener(v -> showDeviceSelectionDialog());
         disconnectButton.setOnClickListener(v -> bluetoothService.disconnect());

         // Tank controls
         leftTankSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
             @Override
             public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                 if (fromUser) {
                     updateTankValues();
                     sendTankCommand();
                 }
             }

             @Override
             public void onStartTrackingTouch(SeekBar seekBar) {}

             @Override
             public void onStopTrackingTouch(SeekBar seekBar) {
                 sendTankCommand();
             }
         });

         rightTankSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
             @Override
             public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                 if (fromUser) {
                     updateTankValues();
                     sendTankCommand();
                 }
             }

             @Override
             public void onStartTrackingTouch(SeekBar seekBar) {}

             @Override
             public void onStopTrackingTouch(SeekBar seekBar) {
                 sendTankCommand();
             }
         });

         tankStopButton.setOnClickListener(v -> {
             leftTankSeekBar.setProgress(200);
             rightTankSeekBar.setProgress(200);
             sendTankCommand();
         });

         // Arm controls - Forward buttons
         clawGrabForward.setOnClickListener(v -> {
             sendArmCommand(0, "f");
             motorStates[0] = true;
             updateMotorButtons();
         });

         clawRotateForward.setOnClickListener(v -> {
             sendArmCommand(1, "f");
             motorStates[1] = true;
             updateMotorButtons();
         });

         middleReachForward.setOnClickListener(v -> {
             sendArmCommand(2, "f");
             motorStates[2] = true;
             updateMotorButtons();
         });

         baseReachForward.setOnClickListener(v -> {
             sendArmCommand(3, "f");
             motorStates[3] = true;
             updateMotorButtons();
         });

         // Arm controls - Backward buttons
         clawGrabBackward.setOnClickListener(v -> {
             sendArmCommand(0, "b");
             motorStates[0] = true;
             updateMotorButtons();
         });

         clawRotateBackward.setOnClickListener(v -> {
             sendArmCommand(1, "b");
             motorStates[1] = true;
             updateMotorButtons();
         });

         middleReachBackward.setOnClickListener(v -> {
             sendArmCommand(2, "b");
             motorStates[2] = true;
             updateMotorButtons();
         });

         baseReachBackward.setOnClickListener(v -> {
             sendArmCommand(3, "b");
             motorStates[3] = true;
             updateMotorButtons();
         });

         stopSelectedMotor.setOnClickListener(v -> {
             sendArmCommand(selectedMotor, "s");
             motorStates[selectedMotor] = false;
             updateMotorButtons();
         });

         stopAllButton.setOnClickListener(v -> {
             bluetoothService.sendCommand(CommandProcessor.stopAll());
             for (int i = 0; i < motorStates.length; i++) {
                 motorStates[i] = false;
             }
             updateMotorButtons();
         });
     }

     private void updateTankValues() {
         int leftValue = leftTankSeekBar.getProgress() - 200;
         int rightValue = rightTankSeekBar.getProgress() - 200;

         leftTankValue.setText(String.valueOf(leftValue));
         rightTankValue.setText(String.valueOf(rightValue));

         // Update color based on direction
         leftTankValue.setTextColor(leftValue > 0 ?
                 getColor(R.color.blue_forward) :
                 (leftValue < 0 ? getColor(R.color.red_backward) :
                 getColor(R.color.gray)));

         rightTankValue.setTextColor(rightValue > 0 ?
                 getColor(R.color.blue_forward) :
                 (rightValue < 0 ? getColor(R.color.red_backward) :
                 getColor(R.color.gray)));
     }

     private void sendTankCommand() {
         int leftSpeed = leftTankSeekBar.getProgress() - 200;
         int rightSpeed = rightTankSeekBar.getProgress() - 200;
         String command = CommandProcessor.tankSetMotors(leftSpeed, rightSpeed);
         bluetoothService.sendCommand(command);
     }

     private void sendArmCommand(int motorId, String action) {
         String command = CommandProcessor.motorCommand(motorId, action);
         bluetoothService.sendCommand(command);
     }

     private void updateMotorButtons() {
         // Enable/disable forward/backward buttons based on motor state
         clawGrabForward.setEnabled(!motorStates[0]);
         clawGrabBackward.setEnabled(!motorStates[0]);
         clawRotateForward.setEnabled(!motorStates[1]);
         clawRotateBackward.setEnabled(!motorStates[1]);
         middleReachForward.setEnabled(!motorStates[2]);
         middleReachBackward.setEnabled(!motorStates[2]);
         baseReachForward.setEnabled(!motorStates[3]);
         baseReachBackward.setEnabled(!motorStates[3]);

         // Update spinner to show current motor status
         String[] motorNames = {"Claw Grab", "Claw Rotate", "Middle Reach", "Base Reach"};
         String status = motorStates[selectedMotor] ? " (MOVING)" : " (STOPPED)";
         motorSpinner.setPrompt(motorNames[selectedMotor] + status);
     }

     private void showDeviceSelectionDialog() {
         if (!bluetoothService.isBluetoothEnabled()) {
             bluetoothService.enableBluetooth();
             Toast.makeText(this, "Please enable Bluetooth", Toast.LENGTH_SHORT).show();
             return;
         }

         Set<BluetoothDevice> pairedDevices = bluetoothAdapter.getBondedDevices();
         List<BluetoothDevice> availableDevices = new ArrayList<>();
         List<String> deviceNames = new ArrayList<>();

         // Add paired devices
         for (BluetoothDevice device : pairedDevices) {
             if (device.getName() != null &&
                 (device.getName().contains("ESP32") ||
                  device.getName().contains("Tank") ||
                  device.getName().contains("Robot"))) {
                 availableDevices.add(device);
                 deviceNames.add(device.getName() + "\n" + device.getAddress());
             }
         }

         if (availableDevices.isEmpty()) {
             Toast.makeText(this, "No paired ESP32 devices found. Please pair first.",
                     Toast.LENGTH_LONG).show();
             return;
         }

         new AlertDialog.Builder(this)
                 .setTitle("Select Tank Robot")
                 .setItems(deviceNames.toArray(new String[0]),
                         (dialog, which) -> {
                             BluetoothDevice selectedDevice = availableDevices.get(which);
                             connectionStatus.setText("Connecting to " + selectedDevice.getName() + "...");
                             bluetoothService.connectToDevice(selectedDevice);
                         })
                 .setNegativeButton("Cancel", null)
                 .show();
     }

     private void checkPermissions() {
         boolean allPermissionsGranted = true;

         for (String permission : REQUIRED_PERMISSIONS) {
             if (ActivityCompat.checkSelfPermission(this, permission)
                     != PackageManager.PERMISSION_GRANTED) {
                 allPermissionsGranted = false;
                 break;
             }
         }

         if (!allPermissionsGranted) {
             ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS,
                     PERMISSION_REQUEST_CODE);
         } else {
             initializeBluetooth();
         }
     }

     private void initializeBluetooth() {
         if (!bluetoothService.isBluetoothSupported()) {
             Toast.makeText(this, "Bluetooth not supported", Toast.LENGTH_LONG).show();
             connectButton.setEnabled(false);
             return;
         }

         if (!bluetoothService.isBluetoothEnabled()) {
             connectionStatus.setText("Bluetooth disabled");
         } else {
             connectionStatus.setText("Ready to connect");
         }
     }

     @Override
     public void onRequestPermissionsResult(int requestCode,
             @NonNull String[] permissions, @NonNull int[] grantResults) {
         super.onRequestPermissionsResult(requestCode, permissions, grantResults);

         if (requestCode == PERMISSION_REQUEST_CODE) {
             boolean allGranted = true;
             for (int result : grantResults) {
                 if (result != PackageManager.PERMISSION_GRANTED) {
                     allGranted = false;
                     break;
                 }
             }

             if (allGranted) {
                 initializeBluetooth();
             } else {
                 Toast.makeText(this,
                         "All permissions are required for Bluetooth functionality",
                         Toast.LENGTH_LONG).show();
             }
         }
     }

     @Override
     protected void onDestroy() {
         super.onDestroy();
         bluetoothService.disconnect();
     }

     @Override
     protected void onStop() {
         super.onStop();
         // Optional: Disconnect when app goes to background
         // bluetoothService.disconnect();
     }
 }
