/* reads the tss.log created by silvercheetah and gives advice.
 *
 * Reads your workout history and data from tss.log generated by silvercheetah
 * and calculates the TSS that is needed day-by-day to achieve optimal fitness
 * while remaining under a given freshness threshold.
 *
 * Also, uses a formula which correlates TSS with time, speed, and FTP during a
 * workout. With this, it can recommend how long you need to cycle at some
 * speed to achieve today's recommended TSS.
 *
 * Pairs well with gitgraph to show workouts as heatmap.
 *
 * Added some unicode flare when you increase your freshhold and FTP.
 *
 * first working code finished on: Thu Feb 28 21:51:24 CST 2019
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

// When you project what your future TSS values should be, you need to add an appendix onto the array read
// from the file, and calculate the required TSS for each of those future days. Even if you only display
// a week of requirements, you need more than 1 week to calculate the TSS values correctly. Therefore,
// APPEND_LEN (where the calculation is done) is 14 days, but the INVISIBLE_INDEX (the number of days
// on the end of the appendix that are not displayed because the values will be incorrect) is 7.
#define APPEND_LEN 14
#define INVISIBLE_APPENDIX 7

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

double sec_goal(double ftp, double speed, double tss);

int main(int argc, char **argv){
    FILE *fp;
    char c;
    unsigned i, j, k, array_size, *duration;
    long long unsigned *ts;
    double *np, *ftp, *ifact, *tss, *ctl, *atl, *tsb;
    double min_tsb;
    char folder_location[4096];

    assert(APPEND_LEN >= INVISIBLE_APPENDIX);

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
    unsigned limit = 0;
    if(array_size > APPEND_LEN * 2)
        limit = array_size - (APPEND_LEN * 2);

    printf("\nTIMESTAMP |  NP   | mins |  FTP  | IF  |  TSS  |  CTL  |  ATL  |  TSB\n");
    for(i = limit; i < array_size; i++){
        printf("%-10llu %7.3lf %5u %8.3lf %5.3lf %7.3lf %7.3lf %7.3lf %6.3lf\n", ts[i], np[i], duration[i] / 60, ftp[i], ifact[i], tss[i], ctl[i], atl[i], tsb[i]);
        if(i == array_size - APPEND_LEN - 1){
            puts("--------------------------------FUTURE--------------------------------");
        }

        if(i == array_size - INVISIBLE_APPENDIX - 1)
            break;
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
    // code below is messy. Fix later.
    long long unsigned current_time = time(NULL) / 86400 * 86400;
    long long unsigned last_time = ts[array_size - APPEND_LEN - 1] / 86400 * 86400;
    if(current_time != last_time){

        double speed = 27.0;
        double tss_goal = tss[array_size - APPEND_LEN];
        double time_goal = sec_goal(curr_ftp, speed, tss_goal);
        // round up.
        if((time_goal - (int)time_goal) >= 0.5)
            time_goal += 0.5;

        puts("\nRecommendations:");
        if(time_goal >= 1)
            printf("progress: %.0lf TSS ≈ %.0lf mins at %.1f km/h.\n", tss_goal, time_goal / 60, speed);
        else
            puts("progress: rest day.");

        // Also calculate a 'maintain goal'. This will be useful if I'm ill but still want to do
        // the minimum to hold my CTL where it is currently.
        double curr_ctl = ctl[array_size - APPEND_LEN - 1];
        time_goal = sec_goal(curr_ftp, speed, curr_ctl);
        // round up.
        if((time_goal - (int)time_goal) >= 0.5)
            time_goal += 0.5;

        if(time_goal >= 1)
            printf("maintain: %.0lf TSS ≈ %.0lf mins at %.1f km/h.\n", curr_ctl, time_goal / 60, speed);
        else
            puts("maintain: rest day.");
    }
    else{
        puts("today is done :)");
        system("gnuplot /home/korgan/code/cyclecoach/gnuplotscript");
    }
    free(ts);
    free(tss);
}

double sec_goal(double ftp, double speed, double tss){
    // using http://www.statskingdom.com/410multi_linear_regression.html
    // last updated: Wed 29 May 2019 09:32:33 PM CDT
    // return (tss + 0.08457 * ftp - 0.07984 * speed - 14.6857) / 0.02234;

    // instead of regression, just realise that you can totally simplify the TSS equation when the speed is constant.
    //sanity check:
    if(speed < 1.0)
        return 0;
    double denom = 0.00472112 * speed * speed * speed + 3.25888 * speed;
    denom *= denom;
    return (tss * 36 * ftp * ftp) / denom;
}
