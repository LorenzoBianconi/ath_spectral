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

enum fft_sample_type {
	FFT_SAMPLE_HT20 = 1,
    FFT_SAMPLE_HT20_40
};

struct fft_sample_tlv {
    enum fft_sample_type type;
    uint16_t length;
}  __attribute__((packed));

struct fft_sample {
    struct fft_sample_tlv tlv;
    uint16_t freq;
    uint64_t tsf;

	union {
		struct {
            int8_t rssi;
            int8_t nf;

            uint8_t data[SPECTRAL_HT20_NUM_BINS];
            uint16_t max_mag;
            uint8_t max_idx;
            uint8_t bitmap_w;
            uint8_t max_exp;
		} ht20;
		struct {
            int8_t lower_rssi;
            int8_t upper_rssi;
            int8_t lower_nf;
            int8_t upper_nf;

            uint8_t data[SPECTRAL_HT20_40_NUM_BINS];
            uint16_t lower_max_mag;
            uint16_t upper_max_mag;
            uint8_t lower_max_idx;
            uint8_t upper_max_idx;
            uint8_t lower_bitmap_w;
            uint8_t upper_bitmap_w;
            uint8_t max_exp;
		} ht20_40;
	};
} __attribute__((packed));

struct scan_sample {
    struct fft_sample data;
    struct scan_sample *next;
};

class AthScan : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit AthScan(QWidget *parent = 0);
    ~AthScan();

private slots:
    int exit();
    int open_scan_file();
    int scale_axis();

private:
    int parse_scan_file(QString);
    int draw_spectrum(quint32, quint32);
    void compute_bin_pwr(struct fft_sample, QPolygonF&);

    Ui::AthScan *ui;
    struct scan_sample *_fft_data;

    quint32 _min_freq, _max_freq;
};

#endif // ATHSCAN_H
