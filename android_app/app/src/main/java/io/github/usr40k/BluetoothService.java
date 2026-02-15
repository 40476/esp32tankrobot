 package io.github.usr40k.esp32tankrobot;
 
 import android.bluetooth.BluetoothAdapter;
 import android.bluetooth.BluetoothDevice;
 import android.bluetooth.BluetoothSocket;
 import android.os.Handler;
 import android.os.Looper;
 import android.util.Log;

 import java.io.IOException;
 import java.io.InputStream;
 import java.io.OutputStream;
 import java.util.UUID;

 public class BluetoothService {
     private static final String TAG = "BluetoothService";
     private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
     private static final int BUFFER_SIZE = 1024;

     private BluetoothAdapter bluetoothAdapter;
     private BluetoothSocket bluetoothSocket;
     private OutputStream outputStream;
     private InputStream inputStream;
     private Thread readThread;
     private boolean isConnected = false;
     private Handler uiHandler;

     public interface ConnectionListener {
         void onConnected(String deviceName);
         void onDisconnected();
         void onMessageReceived(String message);
         void onError(String error);
     }

     private ConnectionListener listener;

     public BluetoothService(Handler handler) {
         this.uiHandler = handler;
         bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
     }

     public void setConnectionListener(ConnectionListener listener) {
         this.listener = listener;
     }

     public boolean isBluetoothSupported() {
         return bluetoothAdapter != null;
     }

     public boolean isBluetoothEnabled() {
         return bluetoothAdapter != null && bluetoothAdapter.isEnabled();
     }

     public void enableBluetooth() {
         if (bluetoothAdapter != null && !bluetoothAdapter.isEnabled()) {
             bluetoothAdapter.enable();
         }
     }

     public void connectToDevice(BluetoothDevice device) {
         new Thread(() -> {
             try {
                 bluetoothSocket = device.createRfcommSocketToServiceRecord(SPP_UUID);
                 bluetoothSocket.connect();
                 outputStream = bluetoothSocket.getOutputStream();
                 inputStream = bluetoothSocket.getInputStream();
                 isConnected = true;

                 startReading();

                 postToUi(() -> {
                     if (listener != null) {
                         listener.onConnected(device.getName());
                     }
                 });

             } catch (IOException e) {
                 Log.e(TAG, "Connection failed: " + e.getMessage());
                 postToUi(() -> {
                     if (listener != null) {
                         listener.onError("Connection failed: " + e.getMessage());
                     }
                 });
                 disconnect();
             }
         }).start();
     }

     private void startReading() {
         readThread = new Thread(() -> {
             byte[] buffer = new byte[BUFFER_SIZE];
             StringBuilder messageBuilder = new StringBuilder();

             while (isConnected) {
                 try {
                     int bytes = inputStream.read(buffer);
                     if (bytes > 0) {
                         String data = new String(buffer, 0, bytes);
                         messageBuilder.append(data);

                         // Check for complete messages (ending with newline)
                         int newLineIndex;
                         while ((newLineIndex = messageBuilder.toString().indexOf("\n")) != -1) {
                             String completeMessage = messageBuilder.substring(0, newLineIndex).trim();
                             messageBuilder.delete(0, newLineIndex + 1);

                             if (!completeMessage.isEmpty()) {
                                 final String message = completeMessage;
                                 postToUi(() -> {
                                     if (listener != null) {
                                         listener.onMessageReceived(message);
                                     }
                                 });
                             }
                         }
                     }
                 } catch (IOException e) {
                     if (isConnected) {
                         postToUi(() -> {
                             if (listener != null) {
                                 listener.onError("Read error: " + e.getMessage());
                             }
                         });
                         disconnect();
                     }
                     break;
                 }
             }
         });
         readThread.start();
     }

     public void sendCommand(String command) {
         if (!isConnected || outputStream == null) {
             return;
         }

         new Thread(() -> {
             try {
                 String fullCommand = command + "\n";
                 outputStream.write(fullCommand.getBytes());
                 outputStream.flush();
             } catch (IOException e) {
                 Log.e(TAG, "Send failed: " + e.getMessage());
                 postToUi(() -> {
                     if (listener != null) {
                         listener.onError("Send failed: " + e.getMessage());
                     }
                 });
                 disconnect();
             }
         }).start();
     }

     public void disconnect() {
         isConnected = false;

         try {
             if (readThread != null) {
                 readThread.interrupt();
             }
             if (inputStream != null) {
                 inputStream.close();
             }
             if (outputStream != null) {
                 outputStream.close();
             }
             if (bluetoothSocket != null) {
                 bluetoothSocket.close();
             }
         } catch (IOException e) {
             Log.e(TAG, "Disconnect error: " + e.getMessage());
         }

         postToUi(() -> {
             if (listener != null) {
                 listener.onDisconnected();
             }
         });
     }

     private void postToUi(Runnable runnable) {
         if (uiHandler != null) {
             uiHandler.post(runnable);
         }
     }
 }
