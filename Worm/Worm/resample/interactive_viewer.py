"""PySide6/PyQtGraph viewer for the actual experiment WAV files."""
import argparse
import sys
import numpy as np
import soundfile as sf
import pyqtgraph as pg
from PySide6 import QtCore, QtWidgets
from audio_core import ROOT, RESULTS, METHODS, FS_IN, FS_OUT, read_input, psd, spectrogram, db_power


class Viewer(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('Audio resampling experiment — PCM sample viewer')
        self.resize(1300, 900)
        self.data = {}
        self.curves = {}
        self.colors = dict(zip(('Original',)+METHODS, ['#dddddd','#66bbff','#ff9933','#66dd88','#ff6688','#bb88ff']))
        central = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(central)
        self.setCentralWidget(central)
        controls = QtWidgets.QHBoxLayout()
        layout.addLayout(controls)
        self.dataset = QtWidgets.QComboBox()
        self.dataset.addItems(['synthetic','recorded'])
        controls.addWidget(self.dataset)
        self.checks = {}
        for name in ('Original',)+METHODS:
            check = QtWidgets.QCheckBox(name)
            check.setChecked(True)
            check.toggled.connect(self.update_visibility)
            self.checks[name] = check
            controls.addWidget(check)
        self.points = QtWidgets.QCheckBox('Sample points (zoom < 30 ms)')
        self.points.setChecked(True)
        self.points.toggled.connect(self.update_symbols)
        controls.addWidget(self.points)
        self.wave = pg.PlotWidget(title='Waveforms — drag to pan, wheel to zoom')
        self.wave.setLabel('bottom','Time',units='s')
        self.wave.setLabel('left','PCM Amplitude')
        self.wave.addLegend()
        self.wave.showGrid(x=True,y=True,alpha=.2)
        layout.addWidget(self.wave, 3)
        self.overview = pg.PlotWidget(title='Drag the region handles, then click Zoom selection')
        self.overview.setLabel('bottom','Time',units='s')
        self.overview.setLabel('left','PCM Amplitude')
        self.overview.setMaximumHeight(150)
        layout.addWidget(self.overview)
        self.region = pg.LinearRegionItem((.2,.21))
        self.overview.addItem(self.region)
        actions = QtWidgets.QHBoxLayout()
        layout.addLayout(actions)
        zoom = QtWidgets.QPushButton('Zoom selection')
        zoom.clicked.connect(self.zoom_selection)
        actions.addWidget(zoom)
        reset = QtWidgets.QPushButton('Full duration')
        reset.clicked.connect(self.reset_view)
        actions.addWidget(reset)
        self.mode = QtWidgets.QComboBox()
        self.mode.addItems(['PSD','Spectrogram'])
        self.mode.currentTextChanged.connect(self.refresh_analysis)
        actions.addWidget(self.mode)
        self.spec_method = QtWidgets.QComboBox()
        self.spec_method.addItems(('Original',)+METHODS)
        self.spec_method.currentTextChanged.connect(self.refresh_analysis)
        actions.addWidget(self.spec_method)
        actions.addWidget(QtWidgets.QLabel('Analysis uses full file; PSD overlays checked methods; spectrogram uses selector.'))
        self.analysis = pg.PlotWidget()
        self.analysis.addLegend()
        layout.addWidget(self.analysis, 3)
        self.cursor = pg.InfiniteLine(angle=90, movable=False, pen=pg.mkPen('#ffffff',style=QtCore.Qt.PenStyle.DashLine))
        self.wave.addItem(self.cursor, ignoreBounds=True)
        self.wave.sigXRangeChanged.connect(self.update_symbols)
        self.proxy = pg.SignalProxy(self.wave.scene().sigMouseMoved, rateLimit=30, slot=self.mouse_moved)
        self.dataset.currentTextChanged.connect(self.load_dataset)
        self.load_dataset()

    def load_dataset(self, *_):
        kind = self.dataset.currentText()
        x, _ = read_input(ROOT/('synthetic_test.wav' if kind=='synthetic' else 'audio_20261029180204.wav'))
        self.data = {'Original': (x,FS_IN)}
        for m in METHODS:
            path = RESULTS/kind/'wav'/f'{m}.wav'
            info = sf.info(path)
            y, fs = sf.read(path,dtype='int16')
            if (fs,info.channels,info.subtype,len(y)) != (FS_OUT,1,'PCM_16',round(len(x)*FS_OUT/FS_IN)):
                raise ValueError(f'Invalid experiment WAV: {path}')
            self.data[m] = (y,fs)
        for curve in self.curves.values():
            self.wave.removeItem(curve)
        self.wave.plotItem.legend.clear()
        self.curves = {}
        for name,(a,fs) in self.data.items():
            c = self.wave.plot(np.arange(len(a))/fs,a.astype(float),name=name,pen=self.colors[name])
            c.setClipToView(True)
            c.setDownsampling(auto=True, method='peak')
            self.curves[name] = c
        if hasattr(self,'overview_curve'):
            self.overview.removeItem(self.overview_curve)
        self.overview_curve = self.overview.plot(np.arange(len(x))/FS_IN,x.astype(float),pen='#aaaaaa')
        self.overview_curve.setDownsampling(auto=True,method='peak')
        self.duration = len(x)/FS_IN
        self.region.setBounds((0,self.duration))
        self.region.setRegion((.2,min(.21,self.duration)))
        self.overview.setXRange(0,self.duration,padding=0)
        # Shared absolute scale across original and all five spectrograms.
        self.spec_cache = {name:spectrogram(a,fs) for name,(a,fs) in self.data.items()}
        self.spec_max = max(float(db_power(p).max()) for _,_,p in self.spec_cache.values())
        self.update_visibility()
        self.reset_view()

    def reset_view(self):
        self.wave.setXRange(0,self.duration,padding=0)
        self.wave.enableAutoRange(axis='y')

    def zoom_selection(self):
        a,b = self.region.getRegion()
        self.wave.setXRange(a,b,padding=0)

    def update_visibility(self, *_):
        for name,curve in self.curves.items():
            curve.setVisible(self.checks[name].isChecked())
        if self.data and hasattr(self,'spec_cache'):
            self.refresh_analysis()

    def update_symbols(self, *_):
        lo,hi = self.wave.viewRange()[0]
        show = self.points.isChecked() and hi-lo <= .03
        for name,curve in self.curves.items():
            curve.setSymbol(('o' if name=='Original' else 'x') if show else None)
            curve.setSymbolSize(6)
            curve.setSymbolPen(self.colors[name])
            curve.setDownsampling(ds=1, auto=not show,method='peak')

    def mouse_moved(self, event):
        pos = event[0]
        if not self.wave.sceneBoundingRect().contains(pos):
            return
        p = self.wave.plotItem.vb.mapSceneToView(pos)
        self.cursor.setPos(p.x())
        values = []
        for name,(a,fs) in self.data.items():
            index = round(p.x()*fs)
            if self.checks[name].isChecked() and 0 <= index < len(a):
                values.append(f'{name}={int(a[index])} @{index/fs:.6f}s')
        self.statusBar().showMessage(f'Cursor {p.x():.6f} s / {p.y():.1f} PCM | '+'; '.join(values))

    def refresh_analysis(self, *_):
        if not self.data or not hasattr(self,'spec_cache'):
            return
        self.analysis.clear()
        self.analysis.plotItem.legend.clear()
        if self.mode.currentText()=='PSD':
            for name,(a,fs) in self.data.items():
                if self.checks[name].isChecked():
                    f,p = psd(a,fs)
                    self.analysis.plot(f/1000,db_power(p),name=name,pen=self.colors[name])
            self.analysis.setLabel('bottom','Frequency',units='kHz')
            self.analysis.setLabel('left','PSD (dB re 1 PCM²/Hz)')
            self.analysis.setTitle('Welch PSD — Hann 40 ms, 50% overlap')
            self.analysis.setXRange(0,8,padding=0)
            self.analysis.enableAutoRange(axis='y')
        else:
            name = self.spec_method.currentText()
            f,t,p = self.spec_cache[name]
            image = pg.ImageItem(axisOrder='row-major')
            z = db_power(p)
            image.setImage(z,levels=(self.spec_max-100,self.spec_max))
            image.setLookupTable(pg.colormap.get('magma',source='matplotlib').getLookupTable())
            dt,df = t[1]-t[0],f[1]-f[0]
            image.setRect(QtCore.QRectF(t[0]-dt/2,(f[0]-df/2)/1000,len(t)*dt,len(f)*df/1000))
            self.analysis.addItem(image)
            self.analysis.setLabel('bottom','Time',units='s')
            self.analysis.setLabel('left','Frequency',units='kHz')
            self.analysis.setTitle(f'{name} | Hann 40 ms / hop 10 ms | shared PSD color range {self.spec_max-100:.1f} to {self.spec_max:.1f} dB re 1 PCM²/Hz')
            self.analysis.setXRange(0,self.duration,padding=0)
            self.analysis.setYRange(0,8,padding=0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--smoke-test',action='store_true',help='Exercise both files, modes and zoom then close')
    args = parser.parse_args()
    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv[:1])
    viewer = Viewer()
    viewer.show()
    if args.smoke_test:
        for dataset in ('synthetic','recorded'):
            viewer.dataset.setCurrentText(dataset)
            viewer.zoom_selection()
            for mode in ('PSD','Spectrogram'):
                viewer.mode.setCurrentText(mode)
                for m in ('Original',)+METHODS:
                    viewer.spec_method.setCurrentText(m)
                    app.processEvents()
            viewer.checks['Linear'].setChecked(False)
            viewer.checks['Linear'].setChecked(True)
            viewer.reset_view()
        QtCore.QTimer.singleShot(800,app.quit)
    return app.exec()


if __name__=='__main__':
    sys.exit(main())
