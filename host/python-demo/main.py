import ctypes
from PyQt5 import QtWidgets, uic
import numpy as np
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt5agg import NavigationToolbar2QT as NavigationToolbar
import matplotlib.pyplot as plt
import sounddevice as sd
import wave
import effects
import sys
from pathlib import Path


class Ui(QtWidgets.QMainWindow):
    def __init__(self):
        super(Ui, self).__init__()
        ui_path = Path(__file__).resolve().parent / "demo.ui"
        self.ui = uic.loadUi(str(ui_path), self)

        self.input_signal = np.zeros(2)
        self.output_signal = np.zeros(2)

        self.sample_rate = 0

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

        plt.rcParams.update({'font.size': 20})
        self.plot()

        self.ui.resetPushButton.clicked.connect(self.reset_handler)
        self.ui.addSignalsPushButton.clicked.connect(self.add_signals_handler)
        self.ui.applyEffectsPushButton.clicked.connect(self.apply_effects_handler)
        self.ui.playInputPushButton.clicked.connect(self.play_input_signal_handler)
        self.ui.playOutputPushButton.clicked.connect(self.play_output_signal_handler)
        self.ui.stopPushButton.clicked.connect(self.stop_playing_handler)
        self.ui.openFilePushButton.clicked.connect(self.open_file_handler)

    def _scale_q(self, value):
        return int(value * 2 ** effects.fixed_point_q)

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

    def _apply_overdrive(self, samples):
        print("Adding overdrive...")
        self.overdrive_runtime.set_overdrive(self.overdrive_param)
        return np.asarray(self.overdrive_runtime.process(samples.tolist()))

    def _apply_echo(self, samples):
        print("Adding echo...")
        delay_samples = int(self.echo_param.delay_samples)
        padded_samples = np.pad(samples, (delay_samples, 0))
        self.echo_runtime.set_echo(self.echo_param)
        return np.asarray(self.echo_runtime.process_echo(padded_samples.tolist(), delay_samples))

    def _apply_compression(self, samples):
        print("Adding compression...")
        self.compression_runtime.set_compression(self.compression_param)
        return np.asarray(self.compression_runtime.process(samples.tolist()))

    def plot(self):
        self.figure.clear()
        ax = self.figure.add_subplot(111)
        ax.plot(self.input_signal.tolist(), label="input")
        ax.plot(self.output_signal.tolist(), label="output")
        ax.legend()
        plt.setp(ax.get_xticklabels(), rotation=30, horizontalalignment='right', fontsize='x-small')

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
            print("Adding sin...")
            freq = 2 * np.pi / self.ui.frequencySpinBox.value()
            amp = self.ui.sinAmplitudeSpinBox.value()
            phase = self.ui.phaseSpinBox.value() * 2 * np.pi / self.ui.frequencySpinBox.value()
            sin = np.asarray([round(amp * np.sin(freq * i + phase), 4) for i in range(num_samples)])
            if len(sin) < len(self.input_signal):
                sin.resize(self.input_signal.shape)
            else:
                self.input_signal.resize(sin.shape)
            self.input_signal += sin

        if self.ui.constantEnabledCheckBox.isChecked():
            value = self.ui.constantValueSpinBox.value()
            constant = value * np.ones(num_samples)
            print(f"Adding constant (value = {value})...")
            if len(constant) < len(self.input_signal):
                constant.resize(self.input_signal.shape)
            else:
                self.input_signal.resize(constant.shape)
            self.input_signal += constant

        if self.ui.impulseEnabledCheckBox.isChecked():
            print("Adding impulse...")
            value = self.ui.impulseValueSpinBox.value()
            delay = self.ui.impulseDelaySpinBox.value()
            if delay > num_samples:
                print("Invalid impulse delay!")
            else:
                impulse = np.zeros(num_samples)
                impulse[delay] = value
                if len(impulse) < len(self.input_signal):
                    impulse.resize(self.input_signal.shape)
                else:
                    self.input_signal.resize(impulse.shape)
                self.input_signal += impulse

        if self.ui.noiseEnabledCheckBox.isChecked():
            print("Adding noise...")
            amp = self.ui.noiseAmplitudeSpinBox.value()
            rng = np.random.default_rng()
            noise = amp * rng.uniform(-amp, amp, num_samples)
            if len(noise) < len(self.input_signal):
                noise.resize(self.input_signal.shape)
            else:
                self.input_signal.resize(noise.shape)
            self.input_signal += noise

        self.output_signal = np.zeros(self.input_signal.shape)

        self.plot()

    def apply_effects_handler(self):
        print("Applying effect(s)...")

        self._update_effect_params()

        # Apply effects
        temp = self.input_signal.copy()

        if self.ui.overdriveEnabledCheckBox.isChecked():
            temp = self._apply_overdrive(temp)

        if self.ui.echoEnabledCheckBox.isChecked():
            temp = self._apply_echo(temp)

        if self.ui.compressionEnabledCheckBox.isChecked():
            temp = self._apply_compression(temp)

        self.output_signal = temp.copy()

        self.plot()

    def play_input_signal_handler(self):
        # Convert to wav file and play
        gain = self.ui.gainDoubleSpinBox.value()
        wav_file = np.array(gain * self.input_signal, dtype=np.int16)
        sample_rate = self.ui.sampleRateSpinBox.value()

        sd.play(wav_file, samplerate=sample_rate, loop=True)

    def play_output_signal_handler(self):
        # Convert to wav file and play
        gain = self.ui.gainDoubleSpinBox.value()
        wav_file = np.array(gain * self.output_signal, dtype=np.int16)
        sample_rate = self.ui.sampleRateSpinBox.value()

        sd.play(wav_file, samplerate=sample_rate, loop=True)

    def stop_playing_handler(self):
        sd.stop()

    def open_file_handler(self):
        file_name, _filter = QtWidgets.QFileDialog.getOpenFileName(self, "Open audio file", ".", "Audio files (*.wav)")

        if file_name is not None:
            file = wave.open(file_name)
            samples = file.getnframes()
            audio = file.readframes(samples)
            self.input_signal = np.frombuffer(audio, dtype=np.int16)
            self.plot()


def main():
    app = QtWidgets.QApplication(sys.argv)
    window = Ui()
    window.show()
    app.exec()


if __name__ == "__main__":
    main()