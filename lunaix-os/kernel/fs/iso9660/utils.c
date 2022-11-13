#include <lunaix/fs/iso9660.h>

struct iso_drecord*
iso9660_get_drecord(struct iso_var_mdu* drecord_mdu)
{
    if (drecord_mdu->len <= sizeof(struct iso_drecord)) {
        return NULL;
    }
    return (struct iso_drecord*)drecord_mdu->content;
}

#define FOUR_DIGIT(x) (x[0] + x[1] * 10 + x[2] * 100 + x[3] * 1000)
#define TWO_DIGIT(x) (x[0] + x[1] * 10)

time_t
iso9660_dt2unix(struct iso_datetime* isodt)
{
    return time_tounix(FOUR_DIGIT(isodt->year),
                       TWO_DIGIT(isodt->month),
                       TWO_DIGIT(isodt->day),
                       TWO_DIGIT(isodt->hour),
                       TWO_DIGIT(isodt->min),
                       TWO_DIGIT(isodt->sec));
}

time_t
iso9660_dt22unix(struct iso_datetime2* isodt2)
{
    return time_tounix(isodt2->year + 1900,
                       isodt2->month,
                       isodt2->day,
                       isodt2->hour,
                       isodt2->min,
                       isodt2->sec);
}