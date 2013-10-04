#ifndef ATHSCAN_H
#define ATHSCAN_H

#include <stdint.h>

#include <QMainWindow>

namespace Ui {
class AthScan;
}

#define SPECTRAL_HT20_NUM_BINS      56
#define SPECTRAL_HT20_40_NUM_BINS   128
#define DELTA   (SPECTRAL_HT20_40_NUM_BINS / 2)

enum nl80211_channel_type {
    NL80211_CHAN_NO_HT,
    NL80211_CHAN_HT20,
    NL80211_CHAN_HT40MINUS,
    NL80211_CHAN_HT40PLUS
};

enum ath_fft_sample_type {
    ATH_FFT_SAMPLE_HT20 = 1,
    ATH_FFT_SAMPLE_HT20_40
};

struct fft_sample_tlv {
    uint8_t type;
    uint16_t length;
} __attribute__((packed));

struct fft_sample_ht20 {
    struct fft_sample_tlv tlv;

    uint8_t max_exp;

    uint16_t freq;
    int8_t rssi;
    int8_t noise;

    uint16_t max_magnitude;
    uint8_t max_index;
    uint8_t bitmap_weight;

    uint64_t tsf;

    uint8_t data[SPECTRAL_HT20_NUM_BINS];
} __attribute__((packed));

struct fft_sample_ht20_40 {
    struct fft_sample_tlv tlv;

    uint8_t max_exp;

    uint16_t freq;

    int8_t lower_rssi;
    int8_t upper_rssi;

    uint64_t tsf;

    int8_t lower_noise;
    int8_t upper_noise;

    uint16_t lower_max_magnitude;
    uint16_t upper_max_magnitude;

    uint8_t lower_max_index;
    uint8_t upper_max_index;

    uint8_t lower_bitmap_weight;
    uint8_t upper_bitmap_weight;

    uint8_t data[SPECTRAL_HT20_40_NUM_BINS];
} __attribute__((packed));

struct scan_sample {
    uint8_t *data;
    struct scan_sample *next;
};

class AthScan : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit AthScan(QWidget *parent = 0);
    ~AthScan();

private slots:
    int clear();
    int open_scan_file();
    int scale_axis();

private:
    int parse_scan_file(QString);
    int draw_spectrum(quint32, quint32);
    int compute_bin_pwr(fft_sample_tlv *, QPolygonF&);

    Ui::AthScan *ui;
    struct scan_sample *_fft_data;

    quint32 _min_freq, _max_freq;
};

#endif // ATHSCAN_H
