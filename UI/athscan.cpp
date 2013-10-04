#include "athscan.h"
#include "ui_athscan.h"

#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QtEndian>
#include <QDebug>

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
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
    _min_freq = 2400;
    _max_freq = 6000;

    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(ui->openButton, SIGNAL(clicked()), this, SLOT(open_scan_file()));
    connect(ui->minFreqSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));
    connect(ui->maxFreqSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));
    connect(ui->minPwrSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));
    connect(ui->maxPwrSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));

    /* init graph parameters*/
    QwtPlotCanvas *canvas = new QwtPlotCanvas();
    canvas->setPalette(Qt::black);
    canvas->setBorderRadius(10);
    ui->fftPlot->setCanvas(canvas);

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
    quint32 min_freq = ~0, max_freq = 0;
    QFile scan_file(file_name);
    if (!scan_file.open(QIODevice::ReadOnly))
        return -1;

    QByteArray buffer = scan_file.readAll();

    int i = 0;
    scan_sample *scan_data = _fft_data;
    while (i < buffer.size()) {
        fft_sample_tlv *tlv = (fft_sample_tlv *)(buffer.data() + i);
        uint8_t *payload = (uint8_t *)(tlv + 1);

        if (tlv->type != ATH_FFT_SAMPLE_HT20 &&
            tlv->type != ATH_FFT_SAMPLE_HT20_40)
            return -1;

        quint32 len = sizeof (fft_sample_tlv) + qFromBigEndian(tlv->length);
        if (len != sizeof(fft_sample_ht20) &&
            len != sizeof(fft_sample_ht20_40))
            return -1;

        scan_sample *data = new(scan_sample);
        if (!data)
            continue;
        data->next = NULL;
        data->data = new(uint8_t[len]);
        if (!data->data)
            continue;
        memcpy(data->data, tlv, len);

        quint16 freq = qFromBigEndian(*(uint16_t *)(payload + 1));

        if (tlv->type == ATH_FFT_SAMPLE_HT20_40) {
            fft_sample_ht20_40 *fft_data, *sample;

            fft_data = (fft_sample_ht20_40 *) data->data;
            sample = (fft_sample_ht20_40 *) tlv;

            fft_data->freq = freq;

            fft_data->lower_max_magnitude = qFromBigEndian(sample->lower_max_magnitude);
            fft_data->upper_max_magnitude = qFromBigEndian(sample->upper_max_magnitude);
        } else {
            fft_sample_ht20 *fft_data, *sample;

            fft_data = (fft_sample_ht20 *) data->data;
            sample = (fft_sample_ht20 *) tlv;

            fft_data->freq = freq;

            fft_data->max_magnitude = qFromBigEndian(sample->max_magnitude);
        }

        if (freq < min_freq)
            min_freq = freq;
        else if (freq > max_freq)
            max_freq = freq;

        if (scan_data)
            scan_data->next = data;
        else
            _fft_data = data;
        scan_data = data;

        i += len;
    }

    scan_file.close();

    _min_freq = min_freq;
    _max_freq = max_freq;

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

int AthScan::compute_bin_pwr(fft_sample_tlv *tlv, QPolygonF &sample)
{
    if (tlv->type == ATH_FFT_SAMPLE_HT20_40) {
        quint32 lower_datasquaresum = 0, upper_datasquaresum = 0;
        fft_sample_ht20_40 *fft_data = (fft_sample_ht20_40 *) tlv;

        for (int i = 0; i < DELTA; i++) {
            int lower_data = fft_data->data[i] << fft_data->max_exp;
            int upper_data = fft_data->data[i + DELTA] << fft_data->max_exp;
            lower_data *= lower_data;
            upper_data *= upper_data;
            lower_datasquaresum += lower_data;
            upper_datasquaresum += upper_data;
        }

        qDebug() << "lower_rssi: " << fft_data->lower_rssi << " lower_nf: " << fft_data->lower_noise;
        qDebug() << "upper_rssi: " << fft_data->upper_rssi << " upper_nf: " << fft_data->upper_noise;

        for (int i = 0; i < DELTA; i++) {
            float freq1 = fft_data->freq - 10.0 + ((20.0 * i) / DELTA);
            float freq2 = fft_data->freq + 10.0 + ((20.0 * i) / DELTA);
            int lower_data = fft_data->data[i] << fft_data->max_exp;
            if (lower_data == 0)
                lower_data = 1;
            int upper_data = fft_data->data[i + DELTA] << fft_data->max_exp;
            if (upper_data == 0)
                upper_data = 1;
            float lower_pwr = fft_data->lower_noise + fft_data->lower_rssi +
                              20 * log10f(lower_data) - log10f(lower_datasquaresum) * 10;
            sample += QPointF(freq1, lower_pwr);
            float upper_pwr = fft_data->upper_noise + fft_data->upper_rssi +
                              20 * log10f(upper_data) - log10f(upper_datasquaresum) * 10;
            sample += QPointF(freq2, upper_pwr);

            qDebug() << "freq1: " << freq1 << " ampl: " << lower_pwr << " freq2: " << freq2 << " ampl: " << upper_pwr;
        }
    } else {
        quint32 datasquaresum = 0;
        fft_sample_ht20 *fft_data = (fft_sample_ht20 *) tlv;
        for (int i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
            int data = fft_data->data[i] << fft_data->max_exp;
            data *= data;
            datasquaresum += data;
        }

        qDebug() << "rssi: " << fft_data->rssi << " nf: " << fft_data->noise;

        for (int i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
            float freq = fft_data->freq - 10.0 + ((20.0 * i) / SPECTRAL_HT20_NUM_BINS);
            int data = fft_data->data[i] << fft_data->max_exp;
            if (data == 0)
                data = 1;
            float pwr = fft_data->noise + fft_data->rssi + 20 * log10f(data) - log10f(datasquaresum) * 10;
            sample += QPointF(freq, pwr);

            qDebug() << "freq: " << freq << "ampl: " << pwr;
        }
    }

    return 0;
}

int AthScan::draw_spectrum(quint32 min_freq, quint32 max_freq)
{
    QPolygonF fft_samples;

    QwtPlotCurve *fft_curve = new QwtPlotCurve();
    fft_curve->setTitle("FFT Samples");
    fft_curve->setPen(Qt::blue, 2);
    fft_curve->setStyle(QwtPlotCurve::Dots);

    ui->fftPlot->setAxisScale(QwtPlot::xBottom, min_freq, max_freq);
    ui->minFreqSpinBox->setValue(min_freq);
    ui->maxFreqSpinBox->setValue(max_freq);

    for (scan_sample *data = _fft_data; data; data = data->next)
        compute_bin_pwr((fft_sample_tlv *) data->data, fft_samples);
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
        draw_spectrum(_min_freq - 20, _max_freq + 20);
    }
    return 0;
}

int AthScan::clear()
{
    while (_fft_data) {
        struct scan_sample *fft_ptr = _fft_data;
        _fft_data = _fft_data->next;
        free(fft_ptr->data);
        free(fft_ptr);
    }

    qApp->exit();

    return 0;
}
