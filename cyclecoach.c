/* reads the tss.log created by silvercheetah and gives advice.
 * recent progress with tss and ftp
 * today's tss goal range
 * you need to able to determine or tell it your freshness threshold so you can
 * plan how much tss to do today without burning out.
 * you also need to be able to tell it a race date so it can taper your workouts accordingly.
 * it needs to be able to adjust the rate at which your tss increases, if that's even a thing
 * anymore. I don't know. I've been coding for 10 hours. I'm tired.
 * Thu Feb 28 21:51:24 CST 2019
 *
 * I'm thnking I could use it like this:
 * ./cyclecoach <days> <freshold> <race date>
 *
 * to see 1 month ahead, I'd use:
 * ./cyclecoach 28
 *
 * to see 1 week ahead with a freshold of -20 and a race date on 4/20:
 * ./cyclecoach 7 -20 2019-04-20
 *
 * Seems though that this might be overburdening a simple program.
 * */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define APPEND_LEN 7
#define TSS_LOG_PATH "/home/korgan/code/silvercheetah/tss.log"

int rolling_average(double* array, double* target, unsigned n, unsigned interval){
    for(unsigned i = 0; i < n; i++){
        if(i < interval){
            if(i == 0)
                target[i] = array[i];
            else
                target[i] = (target[i - 1] * i + array[i]) / (i + 1);
        }
        else
            target[i] = (target[i - 1] * interval + array[i] - array[i - interval]) / interval;
    }
    return 1;
}

int main(int argc, char **argv){
    FILE *fp;
    char c;
    unsigned i, j, k, array_size, *duration;
    long long unsigned *ts;
    double *np, *ftp, *ifact, *tss, *ctl, *atl, *tsb;
    double min_tsb;

    // open tss.log
    if((fp = fopen(TSS_LOG_PATH,"r")) == NULL){
        printf("can't open %s\n", TSS_LOG_PATH);
        exit(1);
    }

    // count lines in tss.log.
    array_size = 0;
    while((c = fgetc(fp)) != EOF)
        if(c == '\n')
            array_size++;
    rewind(fp);

    // skip over header line.
    while((c = fgetc(fp)) != '\n' && c != EOF);

    // if file is empty, we're done.
    if(array_size == 0){
        puts("tss.log is empty :/");
        fclose(fp);
        exit(0);
    }

    // Otherwise, malloc space for tss data + a future block of time.
    if((ts       = malloc((array_size + APPEND_LEN) * sizeof(long long unsigned))) == NULL){ puts("malloc failed"); exit(1); }
    if((duration = malloc((array_size + APPEND_LEN) * sizeof(unsigned))) == NULL){ puts("malloc failed"); exit(1); }
    if((np       = malloc((array_size + APPEND_LEN) * sizeof(double))) == NULL){ puts("malloc failed"); exit(1); }
    if((ftp      = malloc((array_size + APPEND_LEN) * sizeof(double))) == NULL){ puts("malloc failed"); exit(1); }
    if((ifact    = malloc((array_size + APPEND_LEN) * sizeof(double))) == NULL){ puts("malloc failed"); exit(1); }
    if((tss      = malloc((array_size + APPEND_LEN) * sizeof(double))) == NULL){ puts("malloc failed"); exit(1); }
    if((ctl      = malloc((array_size + APPEND_LEN) * sizeof(double))) == NULL){ puts("malloc failed"); exit(1); }
    if((atl      = malloc((array_size + APPEND_LEN) * sizeof(double))) == NULL){ puts("malloc failed"); exit(1); }
    if((tsb      = malloc((array_size + APPEND_LEN) * sizeof(double))) == NULL){ puts("malloc failed"); exit(1); }

    // read tss.log into arrays.
    for(i = 0; i < array_size; i++){
        fscanf(fp, "%llu %lf %u %lf %lf %lf %lf %lf %lf", &ts[i], &np[i], &duration[i], &ftp[i], &ifact[i], &tss[i], &ctl[i], &atl[i], &tsb[i]);
    }
    fclose(fp);

    // point index at date that is <= 7 days previous.
    long long unsigned current_time = time(NULL) / 86400 * 86400;
    for(i = 0; i < array_size; i++){
        if(ts[i] >= current_time - (604800))
            break;
    }
    if(i > 0)
        i--;

    double curr_ftp = ftp[array_size - 1];
    double ftp_change = ftp[array_size - 1] - ftp[i];
    double ctl_change = ctl[array_size - 1] - ctl[i];
    double atl_change = atl[array_size - 1] - atl[i];
    double tsb_change = tsb[array_size - 1] - tsb[i];


    // find lowest ever fresh-hold
    unsigned lowest_freshness = 0;
    for(i = 0; i < array_size; i++){
        if(tsb[i] < tsb[lowest_freshness])
            lowest_freshness = i;
    }

    // get minimum threshold from command line if it's there.
    if(argc == 2){
        // do error check later.
        min_tsb = atof(argv[1]);
    }
    else
        min_tsb = tsb[lowest_freshness];

    printf("freshhold: %.3lf\n", min_tsb);

    // amend array_size to include the future.
    array_size += APPEND_LEN;

    // figure out highest tss than can be done per day to stay within freshhold.
    for(k = array_size - APPEND_LEN; k < array_size; k++){
        for(i = 0; i <= 100; i++){
            // fill appendix with value i.
            for(j = k; j < array_size; j++){
                tss[j] = i;
            }
            // calculate atc, ctl, tsb for appendix.
            rolling_average(tss, ctl, array_size, 42);
            rolling_average(tss, atl, array_size, 7);
            // calculate tsb.
            for(j = 0; j < array_size; j++)
                tsb[j] = ctl[j] - atl[j];

            // find lowest tsb in the appendix.
            unsigned lowest_tsb = k;
            for(j = k + 1; j < array_size; j++){
                if(tsb[j] < tsb[lowest_tsb])
                    lowest_tsb = j;
            }
            if(tsb[lowest_tsb] < min_tsb)
                break;
        }
        if(i)
            i--;

        tss[k] = i;
    }

    // recalculate atc, ctl, tsb for appendix.
    rolling_average(tss, ctl, array_size, 42);
    rolling_average(tss, atl, array_size, 7);
    // recalculate tsb.
    for(j = 0; j < array_size; j++)
        tsb[j] = ctl[j] - atl[j];

    // put in future timestamps.
    for(j = array_size - APPEND_LEN; j < array_size; j++){
        ts[j] = ts[j - 1] + 86400;
    }

    // check it.
    printf("\nTIMESTAMP |  NP   | secs |  FTP  | IF  |  TSS  |  CTL  |  ATL  |  TSB\n");
    for(i = 0; i < array_size; i++){
        printf("%-10llu %7.3lf %6u %7.3lf %5.3lf %7.3lf %7.3lf %7.3lf %6.3lf\n", ts[i], np[i], duration[i], ftp[i], ifact[i], tss[i], ctl[i], atl[i], tsb[i]);
        if(i == array_size - APPEND_LEN - 1)
            puts("--------------------------------FUTURE--------------------------------");
    }

    // print a basic report.
    puts("\n7-DAY REPORT");
    if(ctl_change > 0)
        printf("fitness: +%.1lf :)\n", ctl_change);
    else if(ctl_change <0)
        printf("fitness: %.1lf :(\n", ctl_change);

    if(ftp_change > 0)
        printf("FTP: +%.1lf :)\n", ftp_change);

    double tss_goal = tss[array_size - APPEND_LEN];
    double time_goal = (tss_goal - 19.408 - (0.325 * 25.0) + (0.165 * curr_ftp)) / 1.657;

    if(time_goal >= 1)
        printf("\nRecommendation: %.0lf TSS ≈ %.0lf mins at 25 km/h.\n", tss_goal, time_goal);
    else
        puts("\nRecommendation: rest day.");
}
