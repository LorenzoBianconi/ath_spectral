#include "athscan.h"
#include "ui_athscan.h"

#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QtEndian>
#include <QDebug>
#include <QKeyEvent>

#include <qwt_plot.h>
#include <qwt_legend.h>
#include <qmath.h>

AthScan::AthScan(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AthScan)
{
    ui->setupUi(this);

    _fft_data = NULL;
    _min_freq = 2400;
    _max_freq = 6000;

    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->openButton, SIGNAL(clicked()), this, SLOT(open_scan_file()));
    connect(ui->minFreqSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));
    connect(ui->maxFreqSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));
    connect(ui->minPwrSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));
    connect(ui->maxPwrSpinBox, SIGNAL(editingFinished()), this, SLOT(scale_axis()));

    /* init graph parameters*/
    _canvas = new QwtPlotCanvas();
    _canvas->setPalette(QColor("MidnightBlue"));
    _canvas->setBorderRadius(10);
    ui->fftPlot->setCanvas(_canvas);

    ui->fftPlot->setAxisTitle(QwtPlot::xBottom, "Frequency [MHz]");
    ui->fftPlot->setAxisScale(QwtPlot::xBottom, _min_freq, _max_freq);
    ui->fftPlot->setAxisLabelRotation(QwtPlot::xBottom, -50.0);
    ui->fftPlot->setAxisLabelAlignment(QwtPlot::xBottom, Qt::AlignLeft | Qt::AlignBottom);
    ui->fftPlot->setAxisTitle(QwtPlot::yLeft, "Pwr [dbm]");
    ui->fftPlot->setAxisScale(QwtPlot::yLeft, -96.0, 0.0, 4);

    ui->minFreqSpinBox->setValue(_min_freq);
    ui->maxFreqSpinBox->setValue(_max_freq);
    ui->minPwrSpinBox->setValue(-96);
    ui->maxPwrSpinBox->setValue(0);

    _grid = new QwtPlotGrid();
    _grid->enableXMin(true);
    _grid->setMajorPen(Qt::gray, 0, Qt::DotLine);
    _grid->setMinorPen(Qt::gray, 0, Qt::DotLine);
    _grid->attach(ui->fftPlot);

    quint32 x_border = (_min_freq + _max_freq) / 2;
    qint32 y_border = (ui->minPwrSpinBox->value() + ui->maxPwrSpinBox->value()) / 2;

    _borderV = new QwtPlotMarker();
    _borderV->setLineStyle(QwtPlotMarker::VLine);
    _borderV->setLinePen(Qt::red, 0.0, Qt::DashLine);
    _borderV->setValue(x_border, y_border);
    _borderV->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    QString xlabel;
    xlabel.sprintf("%dMHz", x_border);
    set_label(_borderV, xlabel);
    _borderV->attach(ui->fftPlot);

    _borderH = new QwtPlotMarker();
    _borderH->setLineStyle(QwtPlotMarker::HLine);
    _borderH->setLinePen(Qt::red, 0.0, Qt::DashLine);
    _borderH->setValue(x_border, y_border);
    _borderH->setLabelAlignment(Qt::AlignRight | Qt::AlignBottom);
    QString ylabel;
    ylabel.sprintf("%ddb", y_border);
    set_label(_borderH, ylabel);
    _borderH->attach(ui->fftPlot);

    ui->fftPlot->insertLegend(new QwtLegend());
}

AthScan::~AthScan()
{
    delete ui;
}

void AthScan::set_label(QwtPlotMarker *marker, QString label)
{
    QwtText text(label);
    text.setFont(QFont("Helvetica", 11, QFont::Bold));
    text.setColor(Qt::red);
    marker->setLabel(text);
}

void AthScan::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Up ||
        event->key() == Qt::Key_Down) {
        QString ylabel;
        QPointF value = (event->key() == Qt::Key_Up)
                ? _borderH->value() + QPointF(0, 1)
                : _borderH->value() - QPointF(0, 1);
        ylabel.sprintf("%ddb", (qint32)value.y());
        set_label(_borderH, ylabel);
        _borderH->setValue(value);
    } else if (event->key() == Qt::Key_Left ||
               event->key() == Qt::Key_Right) {
        QString xlabel;
        QPointF value = (event->key() == Qt::Key_Left)
                ? _borderV->value() - QPointF(5, 0)
                : _borderV->value() + QPointF(5, 0);
        xlabel.sprintf("%dMHz", (qint32)value.x());
        set_label(_borderV, xlabel);
        _borderV->setValue(value);
    }
    ui->fftPlot->replot();
}

int AthScan::parse_scan_file(QString file_name)
{
    quint32 min_freq = ~0, max_freq = 0;
    qint32 i = 0;
    QFile scan_file(file_name);

    if (!scan_file.open(QIODevice::ReadOnly))
        return -1;

    QByteArray buffer = scan_file.readAll();

    scan_sample *scan_data = _fft_data;
    while (i < buffer.size()) {
        fft_sample_tlv *tlv = (fft_sample_tlv *)(buffer.data() + i);
        quint8 *payload = (quint8 *)(tlv + 1);

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
        data->data = new(quint8[len]);
        if (!data->data)
            continue;

        memcpy(data->data, tlv, len);
        quint16 freq = qFromBigEndian(*(quint16 *)(payload + 1));

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

        /* compute boundaries */
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

    _min_freq = min_freq - 20;
    _max_freq = max_freq + 20;

    return 0;
}

int AthScan::scale_axis()
{
    qint32 minFreq = ui->minFreqSpinBox->value();
    qint32 maxFreq = (minFreq > ui->maxFreqSpinBox->value()) ?  minFreq + 20 : ui->maxFreqSpinBox->value();
    qint32 minPwr = ui->minPwrSpinBox->value();
    qint32 maxPwr = (minPwr > ui->maxPwrSpinBox->value()) ?  minPwr + 2 : ui->maxPwrSpinBox->value();
    quint32 x_border = (minFreq + maxFreq) / 2;
    qint32 y_border = (minPwr + maxPwr) / 2;

    _borderV->setValue(x_border, y_border);
    QString xlabel;
    xlabel.sprintf("%dMHz", x_border);
    set_label(_borderV, xlabel);
    _borderH->setValue(x_border, y_border);
    QString ylabel;
    ylabel.sprintf("%ddb", y_border);
    set_label(_borderH, ylabel);

    ui->fftPlot->setAxisScale(QwtPlot::xBottom, minFreq, maxFreq);
    ui->fftPlot->setAxisScale(QwtPlot::yLeft, minPwr, maxPwr, 4);

    ui->fftPlot->replot();

    return 0;
}

int AthScan::compute_bin_pwr(fft_sample_tlv *tlv, QPolygonF &sample)
{
    if (tlv->type == ATH_FFT_SAMPLE_HT20_40) {
        quint32 lower_datasquaresum = 0, upper_datasquaresum = 0;
        fft_sample_ht20_40 *fft_data = (fft_sample_ht20_40 *) tlv;

        for (qint32 i = 0; i < DELTA; i++) {
            qint32 lower_data = fft_data->data[i] << fft_data->max_exp;
            qint32 upper_data = fft_data->data[i + DELTA] << fft_data->max_exp;
            lower_data *= lower_data;
            upper_data *= upper_data;
            lower_datasquaresum += lower_data;
            upper_datasquaresum += upper_data;
        }

        for (qint32 i = 0; i < DELTA; i++) {
            float lower_freq, upper_freq;
            if (fft_data->channel_type == NL80211_CHAN_HT40PLUS) {
                lower_freq = fft_data->freq - 10.0 + ((20.0 * i) / DELTA);
                upper_freq = fft_data->freq + 10.0 + ((20.0 * i) / DELTA);
            } else {
                lower_freq = fft_data->freq - 30.0 + ((20.0 * i) / DELTA);
                upper_freq = fft_data->freq - 10.0 + ((20.0 * i) / DELTA);
            }
            qint32 lower_data = fft_data->data[i] << fft_data->max_exp;
            if (lower_data == 0)
                lower_data = 1;
            qint32 upper_data = fft_data->data[i + DELTA] << fft_data->max_exp;
            if (upper_data == 0)
                upper_data = 1;
            float lower_pwr = fft_data->lower_noise + fft_data->lower_rssi +
                              20 * log10f(lower_data) - log10f(lower_datasquaresum) * 10;
            sample += QPointF(lower_freq, lower_pwr);
            float upper_pwr = fft_data->upper_noise + fft_data->upper_rssi +
                              20 * log10f(upper_data) - log10f(upper_datasquaresum) * 10;
            sample += QPointF(upper_freq, upper_pwr);
        }
    } else {
        quint32 datasquaresum = 0;
        fft_sample_ht20 *fft_data = (fft_sample_ht20 *) tlv;
        for (qint32 i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
            qint32 data = fft_data->data[i] << fft_data->max_exp;
            data *= data;
            datasquaresum += data;
        }

        for (qint32 i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
            float freq = fft_data->freq - 10.0 + ((20.0 * i) / SPECTRAL_HT20_NUM_BINS);
            qint32 data = fft_data->data[i] << fft_data->max_exp;
            if (data == 0)
                data = 1;
            float pwr = fft_data->noise + fft_data->rssi + 20 * log10f(data) - log10f(datasquaresum) * 10;
            sample += QPointF(freq, pwr);
        }
    }

    return 0;
}

int AthScan::draw_spectrum(quint32 min_freq, quint32 max_freq)
{
    QPolygonF fft_samples;

    _fft_curve = new QwtPlotCurve();
    _fft_curve->setTitle("FFT Samples");
    _fft_curve->setPen(Qt::green, 2);
    _fft_curve->setStyle(QwtPlotCurve::Dots);

    ui->minFreqSpinBox->setValue(min_freq);
    ui->maxFreqSpinBox->setValue(max_freq);

    for (scan_sample *data = _fft_data; data; data = data->next)
        compute_bin_pwr((fft_sample_tlv *) data->data, fft_samples);

    _fft_curve->setSamples(fft_samples);
    _fft_curve->attach(ui->fftPlot);

    _borderV->setValue((min_freq + max_freq) / 2, (ui->minPwrSpinBox->value() + ui->maxPwrSpinBox->value()) / 2);
    _borderH->setValue((min_freq + max_freq) / 2, (ui->minPwrSpinBox->value() + ui->maxPwrSpinBox->value()) / 2);

    ui->fftPlot->setAxisScale(QwtPlot::xBottom, min_freq, max_freq);

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
        draw_spectrum(_min_freq, _max_freq);
    }
    return 0;
}

int AthScan::close()
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
