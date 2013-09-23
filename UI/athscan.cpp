#include "athscan.h"
#include "ui_athscan.h"

#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QtEndian>
#include <QDebug>

#include <qwt_plot.h>
#include <qwt_plot_grid.h>
#include <qwt_legend.h>
#include <qwt_plot_curve.h>
#include <qmath.h>

AthScan::AthScan(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AthScan)
{
    ui->setupUi(this);

    _fft_data = NULL;

    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(exit()));
    connect(ui->openButton, SIGNAL(clicked()), this, SLOT(open_scan_file()));
    connect(ui->minFreqSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));
    connect(ui->maxFreqSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));
    connect(ui->minPwrSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));
    connect(ui->maxPwrSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));

    /* init graph parameters*/
    ui->fftPlot->setCanvasBackground(Qt::darkGray);
    ui->fftPlot->setAxisTitle(QwtPlot::xBottom, "Frequency [MHz]");
    ui->fftPlot->setAxisScale(QwtPlot::xBottom, 2412, 5825);
    ui->fftPlot->setAxisTitle(QwtPlot::yLeft, "Pwr [dbm]");
    ui->fftPlot->setAxisScale(QwtPlot::yLeft, -95.0, 0.0);

    QwtPlotGrid *grid = new QwtPlotGrid();
    grid->enableXMin(true);
    grid->setMajorPen(Qt::white, 0, Qt::DotLine);
    grid->setMinorPen(Qt::gray, 0, Qt::DotLine);
    grid->attach(ui->fftPlot);

    ui->fftPlot->insertLegend(new QwtLegend());
}

AthScan::~AthScan()
{
    delete ui;
}

int AthScan::parse_scan_file(QString file_name)
{
    QFile scan_file(file_name);
    if (!scan_file.open(QIODevice::ReadOnly))
        return -1;

    QByteArray buffer = scan_file.readAll();

    int i = 0;
    struct scan_sample *scan_data = _fft_data;
    while (i < buffer.size()) {
        struct fft_sample *sample = (struct fft_sample *)(buffer.data() + i);

        if (sample->tlv.type != FFT_SAMPLE_HT20_40 &&
            sample->tlv.type != FFT_SAMPLE_HT20)
            return -1;

        quint32 len = sizeof (struct fft_sample_tlv) + qFromBigEndian(sample->tlv.length);
        if (len != sizeof(struct fft_sample))
            return -1;

        struct scan_sample *data = new(struct scan_sample);
        if (!data)
            continue;

        memset(data, 0, sizeof(*data));
        memcpy(&data->data, &sample->tlv, len);
        data->data.freq = qFromBigEndian(sample->freq);

        if (sample->tlv.type != FFT_SAMPLE_HT20_40) {
            data->data.ht20_40.lower_max_mag = qFromBigEndian(sample->ht20_40.lower_max_mag);
            data->data.ht20_40.upper_max_mag = qFromBigEndian(sample->ht20_40.upper_max_mag);
        } else
            data->data.ht20.max_mag = qFromBigEndian(sample->ht20.max_mag);

        if (scan_data)
            scan_data->next = data;
        else
            _fft_data = data;
        scan_data = data;

        i += len;
    }

    scan_file.close();

    return 0;
}

int AthScan::scale_axis()
{
    quint32 maxFreq = (ui->maxFreqSpinBox->value() > ui->minFreqSpinBox->value())
            ?  ui->maxFreqSpinBox->value() + 1 : ui->minFreqSpinBox->value();
    qint32 maxPwr = (ui->minPwrSpinBox->value() > ui->maxPwrSpinBox->value())
            ?  ui->minPwrSpinBox->value() + 1 : ui->maxPwrSpinBox->value();

    ui->fftPlot->setAxisScale(QwtPlot::xBottom, ui->minFreqSpinBox->value(), maxFreq);
    ui->fftPlot->setAxisScale(QwtPlot::yLeft, ui->minPwrSpinBox->value(), maxPwr);

    ui->fftPlot->replot();

    return 0;
}

void AthScan::compute_bin_pwr(struct fft_sample fft_sample, QPolygonF &sample)
{
    if (fft_sample.tlv.type == FFT_SAMPLE_HT20_40) {
        quint32 lower_datasquaresum = 0, upper_datasquaresum = 0;
        for (int i = 0; i < SPECTRAL_HT20_40_NUM_BINS / 2; i++) {
            int lower_data = fft_sample.ht20_40.data[i] << fft_sample.ht20_40.max_exp;
            int upper_data = fft_sample.ht20_40.data[i + SPECTRAL_HT20_40_NUM_BINS / 2] << fft_sample.ht20_40.max_exp;
            lower_data *= lower_data;
            upper_data *= upper_data;
            lower_datasquaresum += lower_data;
            upper_datasquaresum += upper_data;
        }
        for (int i = 0; i < SPECTRAL_HT20_40_NUM_BINS / 2; i++) {
        }
    } else {
        quint32 datasquaresum = 0;
        for (int i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
            int data = fft_sample.ht20.data[i] << fft_sample.ht20.max_exp;
            data *= data;
            datasquaresum += data;
        }

        for (int i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
            float freq = fft_sample.freq - 10.0 + ((20.0 * i) / SPECTRAL_HT20_NUM_BINS);
            int data = fft_sample.ht20.data[i] << fft_sample.ht20.max_exp;
            if (data == 0)
                data = 1;
            float pwr = fft_sample.ht20.nf + fft_sample.ht20.rssi + 20 * log10f(data) - log10f(datasquaresum) * 10;
            sample += QPointF(freq, pwr);
        }
    }
}

int AthScan::draw_spectrum()
{
    QPolygonF fft_samples;

    QwtPlotCurve *fft_curve = new QwtPlotCurve();
    fft_curve->setTitle("FFT Samples");
    fft_curve->setPen(Qt::blue, 4);
    fft_curve->setStyle(QwtPlotCurve::Dots);

    for (struct scan_sample *data = _fft_data; data; data = data->next)
        compute_bin_pwr(data->data, fft_samples);
    fft_curve->setSamples(fft_samples);
    fft_curve->attach(ui->fftPlot);

    ui->fftPlot->replot();

    return 0;
}

int AthScan::open_scan_file()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr(""));
    if (!fileName.isEmpty()) {
        if (parse_scan_file(fileName) < 0) {
            QMessageBox::information(0,"error","error parsing fft data");
            return -1;
        }
        draw_spectrum();
    }
    return 0;
}

int AthScan::exit()
{
    while (_fft_data) {
        struct scan_sample *fft_ptr = _fft_data;
        _fft_data = _fft_data->next;
        free(fft_ptr);
    }

    qApp->exit();

    return 0;
}
