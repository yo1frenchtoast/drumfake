package org.drumfreake.app;

import android.content.Context;
import android.content.pm.PackageManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import org.libsdl.app.SDLActivity;

// Loads libSDL3.so and libmain.so (the Drumf(re)ake app) and runs SDL_main().
// Also opens every MIDI device (USB, other apps) and forwards what they send
// to the native MIDI input (src/io/midi_android.cc).
public class DrumfreakeActivity extends SDLActivity {
    private MidiManager midiManager;
    private final Map<Integer, MidiDevice> midiDevices = new HashMap<>();

    private static native void nativeMidi(int source, byte[] data, int offset, int count);
    private static native void nativeMidiPorts(int count);

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (!getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI)) {
            return;
        }
        midiManager = (MidiManager) getSystemService(Context.MIDI_SERVICE);
        Handler handler = new Handler(Looper.getMainLooper());
        for (MidiDeviceInfo info : midiManager.getDevices()) {
            openMidiDevice(info, handler);
        }
        midiManager.registerDeviceCallback(new MidiManager.DeviceCallback() {
            @Override
            public void onDeviceAdded(MidiDeviceInfo info) {
                openMidiDevice(info, handler);
            }

            @Override
            public void onDeviceRemoved(MidiDeviceInfo info) {
                closeMidiDevice(info.getId());
            }
        }, handler);
    }

    @Override
    protected void onDestroy() {
        for (MidiDevice device : midiDevices.values()) {
            try {
                device.close();
            } catch (IOException e) {
                // Closing anyway.
            }
        }
        midiDevices.clear();
        super.onDestroy();
    }

    private void openMidiDevice(MidiDeviceInfo info, Handler handler) {
        // Output ports are where a device sends its data.
        if (info.getOutputPortCount() == 0 || midiDevices.containsKey(info.getId())) {
            return;
        }
        midiManager.openDevice(info, device -> {
            if (device == null) {
                return;
            }
            midiDevices.put(info.getId(), device);
            for (int i = 0; i < info.getOutputPortCount(); ++i) {
                MidiOutputPort port = device.openOutputPort(i);
                if (port == null) {
                    continue;
                }
                final int source = info.getId() * 16 + i;
                port.connect(new MidiReceiver() {
                    @Override
                    public void onSend(byte[] data, int offset, int count, long timestamp) {
                        nativeMidi(source, data, offset, count);
                    }
                });
            }
            nativeMidiPorts(midiDevices.size());
        }, handler);
    }

    private void closeMidiDevice(int id) {
        MidiDevice device = midiDevices.remove(id);
        if (device != null) {
            try {
                device.close();
            } catch (IOException e) {
                // Already gone.
            }
        }
        nativeMidiPorts(midiDevices.size());
    }
}
