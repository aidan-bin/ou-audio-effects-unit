import sys
import wave
from pathlib import Path

import effects
import matplotlib.pyplot as plt
import numpy as np
import sounddevice as sd
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt5agg import NavigationToolbar2QT as NavigationToolbar
from PyQt5 import QtWidgets, uic


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        ui_path = Path(__file__).resolve().parent / "demo.ui"
        self.ui = uic.loadUi(str(ui_path), self)

        self.input_signal = np.zeros(2)
        self.output_signal = np.zeros(2)

        self.overdrive_param = effects.OverdriveParam()
        self.echo_param = effects.EchoParam()
        self.compression_param = effects.CompressionParam()
        self.overdrive_runtime = effects.EffectRuntime(effects.EFFECT_TYPE_OVERDRIVE)
        self.echo_runtime = effects.EffectRuntime(effects.EFFECT_TYPE_ECHO)
        self.compression_runtime = effects.EffectRuntime(effects.EFFECT_TYPE_COMPRESSION)

        self.figure = plt.figure()
        self.canvas = FigureCanvas(self.figure)
        self.toolbar = NavigationToolbar(self.canvas, self)
        self.layout = QtWidgets.QVBoxLayout()
        self.layout.addWidget(self.canvas)
        self.layout.addWidget(self.toolbar)
        self.ui.frame.setLayout(self.layout)

        plt.rcParams.update({"font.size": 20})
        self.plot()

        self.ui.resetPushButton.clicked.connect(self.reset_handler)
        self.ui.addSignalsPushButton.clicked.connect(self.add_signals_handler)
        self.ui.applyEffectsPushButton.clicked.connect(self.apply_effects_handler)
        self.ui.playInputPushButton.clicked.connect(self.play_input_signal_handler)
        self.ui.playOutputPushButton.clicked.connect(self.play_output_signal_handler)
        self.ui.stopPushButton.clicked.connect(self.stop_playing_handler)
        self.ui.openFilePushButton.clicked.connect(self.open_file_handler)

    def _scale_q(self, value):
        return int(value * 2**effects.FIXED_POINT_Q)

    def _update_effect_params(self):
        self.overdrive_param.level = self.ui.overdriveLevelSpinBox.value()
        self.overdrive_param.gain = self._scale_q(self.ui.overdriveGainSpinBox.value())
        self.overdrive_param.tone = self._scale_q(self.ui.overdriveToneSpinBox.value())
        self.overdrive_param.mix = self._scale_q(self.ui.overdriveMixSpinBox.value())

        self.echo_param.delay_samples = self.ui.echoDelaySpinBox.value()
        self.echo_param.pre_delay = self.ui.echoPreDelaySpinBox.value()
        self.echo_param.density = self._scale_q(self.ui.echoDensitySpinBox.value())
        self.echo_param.attack = self._scale_q(self.ui.echoAttackSpinBox.value())
        self.echo_param.decay = self._scale_q(self.ui.echoDecaySpinBox.value())

        self.compression_param.threshold = self.ui.compressionThresholdSpinBox.value()
        self.compression_param.ratio = self._scale_q(self.ui.compressionRatioSpinBox.value())

    def _apply_standard_effect(self, label, runtime, setter, params, samples):
        print(f"Adding {label}...")
        setter(params)
        return np.asarray(runtime.process(samples.tolist()))

    def _apply_echo_effect(self, samples):
        print("Adding echo...")
        delay_samples = int(self.echo_param.delay_samples)
        padded_signal = np.pad(samples, (delay_samples, 0))
        self.echo_runtime.set_echo(self.echo_param)
        return np.asarray(self.echo_runtime.process_echo(padded_signal.tolist(), delay_samples))

    def _append_signal(self, signal, component):
        target_len = max(len(signal), len(component))
        signal_padded = np.pad(signal, (0, target_len - len(signal)))
        component_padded = np.pad(component, (0, target_len - len(component)))
        return signal_padded + component_padded

    def _add_sin(self, num_samples):
        print("Adding sin...")
        freq = 2 * np.pi / self.ui.frequencySpinBox.value()
        amp = self.ui.sinAmplitudeSpinBox.value()
        phase = self.ui.phaseSpinBox.value() * 2 * np.pi / self.ui.frequencySpinBox.value()
        component = np.asarray(
            [round(amp * np.sin(freq * i + phase), 4) for i in range(num_samples)]
        )
        self.input_signal = self._append_signal(self.input_signal, component)

    def _add_constant(self, num_samples):
        value = self.ui.constantValueSpinBox.value()
        print(f"Adding constant (value = {value})...")
        component = value * np.ones(num_samples)
        self.input_signal = self._append_signal(self.input_signal, component)

    def _add_impulse(self, num_samples):
        print("Adding impulse...")
        value = self.ui.impulseValueSpinBox.value()
        delay = self.ui.impulseDelaySpinBox.value()
        if delay >= num_samples:
            print("Invalid impulse delay!")
            return

        component = np.zeros(num_samples)
        component[delay] = value
        self.input_signal = self._append_signal(self.input_signal, component)

    def _add_noise(self, num_samples):
        print("Adding noise...")
        amp = self.ui.noiseAmplitudeSpinBox.value()
        rng = np.random.default_rng()
        component = amp * rng.uniform(-amp, amp, num_samples)
        self.input_signal = self._append_signal(self.input_signal, component)

    def _play_signal(self, signal):
        gain = self.ui.gainDoubleSpinBox.value()
        wav_file = np.array(gain * signal, dtype=np.int16)
        sample_rate = self.ui.sampleRateSpinBox.value()
        sd.play(wav_file, samplerate=sample_rate, loop=True)

    def _load_wave_file(self, file_name):
        with wave.open(file_name) as audio_file:
            samples = audio_file.getnframes()
            audio = audio_file.readframes(samples)
        self.input_signal = np.frombuffer(audio, dtype=np.int16)
        self.plot()

    def plot(self):
        self.figure.clear()
        ax = self.figure.add_subplot(111)
        ax.plot(self.input_signal.tolist(), label="input")
        ax.plot(self.output_signal.tolist(), label="output")
        ax.legend()
        plt.setp(ax.get_xticklabels(), rotation=30, horizontalalignment="right", fontsize="x-small")

        self.canvas.draw()

    def reset_handler(self):
        print("Resetting...")
        sd.stop()
        self.input_signal = np.zeros(2)
        self.output_signal = np.zeros(2)
        self.overdrive_runtime.reset()
        self.echo_runtime.reset()
        self.compression_runtime.reset()
        self.plot()

    def add_signals_handler(self):
        num_samples = self.ui.numberOfSamplesSpinBox.value()
        print(f"Adding signal(s) ({num_samples} samples)...")

        if self.ui.sinEnabledCheckBox.isChecked():
            self._add_sin(num_samples)

        if self.ui.constantEnabledCheckBox.isChecked():
            self._add_constant(num_samples)

        if self.ui.impulseEnabledCheckBox.isChecked():
            self._add_impulse(num_samples)

        if self.ui.noiseEnabledCheckBox.isChecked():
            self._add_noise(num_samples)

        self.output_signal = np.zeros(self.input_signal.shape)

        self.plot()

    def apply_effects_handler(self):
        print("Applying effect(s)...")

        self._update_effect_params()

        temp = self.input_signal.copy()

        if self.ui.overdriveEnabledCheckBox.isChecked():
            temp = self._apply_standard_effect(
                "overdrive",
                self.overdrive_runtime,
                self.overdrive_runtime.set_overdrive,
                self.overdrive_param,
                temp,
            )

        if self.ui.echoEnabledCheckBox.isChecked():
            temp = self._apply_echo_effect(temp)

        if self.ui.compressionEnabledCheckBox.isChecked():
            temp = self._apply_standard_effect(
                "compression",
                self.compression_runtime,
                self.compression_runtime.set_compression,
                self.compression_param,
                temp,
            )

        self.output_signal = temp.copy()

        self.plot()

    def play_input_signal_handler(self):
        self._play_signal(self.input_signal)

    def play_output_signal_handler(self):
        self._play_signal(self.output_signal)

    def stop_playing_handler(self):
        sd.stop()

    def open_file_handler(self):
        file_name, _filter = QtWidgets.QFileDialog.getOpenFileName(
            self, "Open audio file", ".", "Audio files (*.wav)"
        )

        if file_name:
            self._load_wave_file(file_name)


def main():
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow()
    window.show()
    app.exec()


if __name__ == "__main__":
    main()
