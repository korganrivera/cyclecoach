/* reads the tss.log created by silvercheetah and gives advice.
 * recent progress with tss and ftp
 * today's tss goal range
 * you need to able to determine or tell it your freshness threshold so you can
 * plan how much tss to do today without burning out.
 * you also need to be able to tell it a race date so it can taper your workouts accordingly.
 * it needs to be able to adjust the rate at which your tss increases, if that's even a thing
 * anymore. I don't know. I've been coding for 10 hours. I'm tired.
 * Thu Feb 28 21:51:24 CST 2019
 * */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define APPEND_LEN 10

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
    char folder_location[4096];

    // open config file.
    if((fp = fopen("/home/korgan/code/cyclecoach/config","r")) == NULL){
        printf("can't find /home/korgan/code/cyclecoach/config file. creating...");
        if((fp = fopen("/home/korgan/code/cyclecoach/config","w")) == NULL){
            printf("failed.\n");
            exit(1);
        }

        // set ./wahoo_csv_files as default location of your csv files.
        fprintf(fp, "/PATH/TO/TSS/LOG/GOES/HERE/tss.log");
        fclose(fp);
        puts("done.\nPut path to tss.log in config file and run cyclecoach again.");
        exit(0);
    }

    // get folder location from config file.
    fgets(folder_location, 4098, fp);
    fclose(fp);
    folder_location[strcspn(folder_location, "\n")] = 0;

    // open tss.log
    if((fp = fopen(folder_location, "r")) == NULL){
        printf("can't open %s\n", folder_location);
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

    // get minimum threshold from command line if it's there.
    if(argc == 2){
        // do error check later.
        min_tsb = atof(argv[1]);
    }
    else{
        // find lowest ever fresh-hold
        unsigned lowest_freshness = 0;
        for(i = 0; i < array_size; i++){
            if(tsb[i] < tsb[lowest_freshness])
                lowest_freshness = i;
        }
        min_tsb = tsb[lowest_freshness];
    }

    printf("freshhold: %.3lf\n", min_tsb);

    // grab last ftp value before amending array_size.
    double curr_ftp = ftp[array_size - 1];

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

    // recalculate atc, ctl, tsb for appendix. So this is overkill since 6 out
    // of 7 of them have already been calculated in the above process, but the
    // last one won't be correct. Instead of just fixing that last one, I'll
    // just do this. Makes it easier if I were to adjust APPEND_LEN later.
    rolling_average(tss, ctl, array_size, 42);
    rolling_average(tss, atl, array_size, 7);
    // recalculate tsb.
    for(j = 0; j < array_size; j++)
        tsb[j] = ctl[j] - atl[j];

    // put in future timestamps.
    for(j = array_size - APPEND_LEN; j < array_size; j++){
        ts[j] = ts[j - 1] + 86400;
    }

    // display it.

    // Only print the last ~20 entries.
    unsigned limit = 0;
    if(array_size > APPEND_LEN * 2)
        limit = array_size - (APPEND_LEN * 2);

    printf("\nTIMESTAMP |  NP   | secs |  FTP  | IF  |  TSS  |  CTL  |  ATL  |  TSB\n");
    for(i = limit; i < array_size; i++){
        printf("%-10llu %7.3lf %6u %7.3lf %5.3lf %7.3lf %7.3lf %7.3lf %6.3lf\n", ts[i], np[i], duration[i], ftp[i], ifact[i], tss[i], ctl[i], atl[i], tsb[i]);
        if(i == array_size - APPEND_LEN - 1)
            puts("--------------------------------FUTURE--------------------------------");
    }

    // figure out longest streak.
    unsigned streak = 0;
    unsigned longest = 0;
    for(i = 0; i < array_size - APPEND_LEN; i++){
        if(tss[i] >= 1.0){
            streak++;
        }
        else{
            if(streak > longest)
                longest = streak;
            streak = 0;
        }
    }

    // figure out current streak;
    streak = 0;
    for(i = array_size - APPEND_LEN - 1; tss[i] >= 1.0; i--)
        streak++;

    if(streak > longest)
        longest = streak;

    printf("\nlongest streak: %u\ncurrent streak: %u\n", longest, streak);

    // detect an FTP increase and report if there is one.
    if(ftp[array_size - APPEND_LEN - 1] > ftp[array_size - APPEND_LEN - 2])
        puts("New FTP!       🌈");

    // If today's freshhold is the same as the minimum freshhold, then I'll
    // *assume* that today's workout was the one that set it.
    if(min_tsb == tsb[array_size - APPEND_LEN - 1])
        puts("New freshhold! 🌈");

    free(duration);
    free(np);
    free(ftp);
    free(ifact);
    free(ctl);
    free(atl);
    free(tsb);

    // print a basic report.
    // fix this: gives wrong report if today doesn't have a workout.
    // instead, maybe compare last weeks average with this week's.
    // or, compare linear model of this and last week's fitness.

    // If you've already worked out today, then no advice needed. Otherwise,
    // recommend a tss to aim for today.
    long long unsigned current_time = time(NULL) / 86400 * 86400;
    long long unsigned last_time = ts[array_size - APPEND_LEN - 1] / 86400 * 86400;
    if(current_time != last_time){

        double speed = 27.0;
        double tss_goal = tss[array_size - APPEND_LEN];
        double time_goal = (tss_goal - 15.543 - 0.0504664*speed + 0.090868*curr_ftp)/1.464918;

        if(time_goal >= 1)
            printf("\nRecommendation: %.0lf TSS ≈ %.0lf mins at %.1f km/h.\n", tss_goal, time_goal, speed);
        else
            puts("\nRecommendation: rest day.");
    }
    else
        puts("today is done :)");
    free(ts);
    free(tss);
}
