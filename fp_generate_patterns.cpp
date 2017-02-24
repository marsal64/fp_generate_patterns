//============================================================================
// Name        : fp_generate_patterns.cpp
// Author      : Martin Saly
// Version     : 1.0
// Description : Fingerprints processing phase 1 - patterns for analysis
//============================================================================


/*
standalone program version to be run using e.g.:
./fp_generate_patterns < testdata.csv > datalog.csv

where testdata.csv has expected structure timestamp; value, e.g.:
10-03-2016 15:19:20.729915 ;   68998
10-03-2016 15:19:20.729979 ;   69058
10-03-2016 15:19:20.730043 ;   68808

After processing, following data are outputted:

lineid - current line (measurement point)
timestamp - timestamp value
meas - measured value
diff - difference of the measured value from the previous value
diffavg - current average absolute value difference  value used for thresholding
isdetect - detection of alarm in progress (counting, see NUMBER_OF_POINTS_TO_ALARM)
isalarm - if alarm, 1 is printed, else 0
iswait - if waiting status, 1 is printed (see WAIT_STATE_USEC), else 0
patternid - if no pattern recognition status, prints 0 else prints sequential number of recognized pattern at each related line

 */

#include <iostream>
#include <string>
#include <iomanip>
#include <sys/time.h>

// Value of the initial absolute difference between subsequent points representing noise
// The value should be set based on real average value
// This value is used as the initial value for avgdiff variable
#define INITIAL_AVG_DIFF 200

// How many subsequent points need to have greater difference between them
// than avgdiff * MULTIPLICATOR_TO_DETECT to raise alarm
#define NUMBER_OF_POINTS_TO_ALARM 5 // must be >= 1

// How many microseconds must elapse between detected alarm to detect subsequent alarm
// e.g. 5 secs = 5000000
#define WAIT_STATE_USEC 1000000

// Multiplicator to multiplicate diffavg value to set the threshold for alarm detection
#define MULTIPLICATOR_TO_DETECT 10

// Smoothing constant for subsequent diffavg amendments, must be >= 1
// higher value means lower sensitivity to average diff value change
#define N_AMEND_AVGDIFF 500

// Sampling parameter: uses each nth input value for evaluation
// If SAMPLE_EACH == 1, takes every value (no sampling)
#define SAMPLE_EACH 1

// Max pattern length: how long pattern recognition can last in microseconds
// If PATTERN_STATE_USEC == 100000, takes 0.1 sec pattern
#define PATTERN_STATE_USEC 250000




// prototype
std::string trim (const std::string&);


// structure for time storing

int main(int argc, char* argv[]) {

	// variable for sampling
	int sampling = SAMPLE_EACH;
	float diffavg = INITIAL_AVG_DIFF;  // initial value of average noise difference
	int number_of_points_to_alarm = NUMBER_OF_POINTS_TO_ALARM;
	int wait_state_usec = WAIT_STATE_USEC;
	int multiplicator_to_detect = MULTIPLICATOR_TO_DETECT;
	int n_amend_avgdiff = N_AMEND_AVGDIFF;
	int pattern_state_usec = PATTERN_STATE_USEC;


	// arguments evaluation
	// program to be called with either none or all six integer arguments in the order:

	/* SAMPLE_EACH
	 * INITIAL_AVG_DIFF
	 * NUMBER_OF_POINTS_TO_ALARM
	 * WAIT_STATE_USEC
	 * MULTIPLICATOR_TO_DETECT
	 * N_AMEND_AVGDIFF
	 */


	// if at least one argument passed, evaluate number of them
	if (argc > 1 and argc < 8) {
		std::cerr << "Arguments error: must pass 7 integer arguments or none" << std::endl;
		std::cerr << "Number of arguments passed: " << argc - 1 << std::endl;
		std::cerr << "Program terminated" << std::endl;
		return 1;
	}

	// parse arguments
	if (argc == 8) {
		try {
		sampling = atoi(argv[1]);
		diffavg = atoi(argv[2]);
		number_of_points_to_alarm = atoi(argv[3]);
		wait_state_usec = atoi(argv[4]);
		multiplicator_to_detect = atoi(argv[5]);
		n_amend_avgdiff = atoi(argv[6]);
		pattern_state_usec = atoi(argv[7]);
		} catch (const std::exception &exc) {
			std::cerr << "Arguments parsing error (must pass 7 integer arguments)";
			std::cerr << exc.what() << std::endl;
			return 1;
		}
	}

	// verify (somehow) values of arguments
	if (sampling < 1 ||
		diffavg < 1 ||
		number_of_points_to_alarm < 1 ||
		wait_state_usec < 1 ||
		multiplicator_to_detect < 1 ||
		n_amend_avgdiff < 1 ||
		pattern_state_usec < 1) {
	std::cerr << std::endl << "Invalid argument(s) value(s):" << std::endl;
	std::cerr << "=============================" << std::endl;
	std::cerr << "sample_each: " << sampling << std::endl;
	std::cerr << "initial_avg_diff: " << diffavg << std::endl;
	std::cerr << "number_of_points_to_alarm: " << number_of_points_to_alarm << std::endl;
	std::cerr << "wait_state_usec: " << wait_state_usec << std::endl;
	std::cerr << "multiplicator_to_detect: " << multiplicator_to_detect << std::endl;
	std::cerr << "n_amend_avgdiff: " << n_amend_avgdiff << std::endl;
	std::cerr << "pattern_state_usec: " << pattern_state_usec << std::endl << std::endl;
	std::cerr << "Exiting..." << std::endl << std::endl;

	// error exit
	return 1;
	}



	// start processing //

	// init sample count
	int cursample = sampling;

	// variables for read data and parsing
	std::string lineread, p1, p2;
	int d,m,y,h,mi,s,ms;


	// helper variable for string char conversion
	char* c;
	c = new char[100];

	// variables related to alarm
	short isalarm = 0;
	short iswait = 0;
	int numthresholded = number_of_points_to_alarm;

	// variables for alarm delay calculation
	struct timeval lasttime = {2147483646,0}; // large enough
	struct timeval curtime = lasttime;
	struct timeval alarmraisetime = {0,0};    // small enough
	struct timeval patternraisetime = {0,0};    // small enough

	// helper tm structure for conversion
	struct tm t = {0};  // Initalize to all 0's

	// helper position variable
	int pos;

	// variable to count number of input lines
	long long lineid=0;

	// values for calculations (mind type)
	float curval,lastval; // current and last value
	float diff = 0; // difference from previous value, absolute value
	float diffnoabs = 0; // noabsvalue

	// patterns related variables
	int patternid = 0 ;
	short ispattern = 0; // status variable for pattern tracking

	// output header
	std::cout << "lineid;timestamp;meas;diff;curavg;isdetect;isalarm;iswait;patternid" << std::endl ;

	while (std::getline(std::cin,lineread))	{

		// debug output: copy of input line
		// std::cout << std::endl << lineread << std::endl;


		// sampling?
		if (cursample-- > 1) {

			// debug
			// std::cout << "previous line sampled out" << std::endl;
			continue;
		}
		else
			cursample = sampling;

		// parses input line
		pos = lineread.find(';');
		p1 = trim(lineread.substr(0,pos));  // parse and trim first before ;, i.e. timestamp
		p2 = trim(lineread.erase(0,pos + 1)); // parse and trim measured value

		// convert string to char to be used by sscanf
		std::copy(p1.begin(), p1.end(), c);
		c[p1.size()] = '\0'; // terminating 0

		// parse using sscanf
		// 10-03-2016 15:19:20.729915
		sscanf(c,"%d-%d-%d %d:%d:%d.%d", &d, &m, &y, &h, &mi, &s, &ms);

		// fill current time
		t.tm_year = y-1900;t.tm_mon = m;t.tm_mday = d;t.tm_hour = h;t.tm_min = mi;t.tm_sec = s;
		curtime.tv_sec = mktime(&t); curtime.tv_usec=ms;

		lasttime = curtime;

		// take current value to curval
		curval = std::stod(p2);

		// increments lineid and copies last values for the first line
		if (!lineid++) lastval = curval;


		// calculate diff as abs value
		diffnoabs = curval - lastval;
		diff = abs(diffnoabs);

		// pattern evaluation
		if (ispattern == 1) {
			// is in "pattern" state

			// debug
			// std::cout << "pattern recognition" << std::endl;
			// std::cout << "patterndiff: " << (curtime.tv_sec - patternraisetime.tv_sec) * 1000000 + (curtime.tv_usec - patternraisetime.tv_usec) << std::endl;

			if (((curtime.tv_sec - patternraisetime.tv_sec) * 1000000) + (curtime.tv_usec - patternraisetime.tv_usec) > pattern_state_usec )
							ispattern = 0; // reset pattern period

		}


		//verify if waiting period after previously detected alarm
		if (iswait == 1 ) {

			// is in "wait" state

			// after entering the wait state, reset the alarm status
			// may change this behavior if needed
			isalarm = 0;

			// debug
			// std::cout << "alarmdiff: " << (curtime.tv_sec - alarmraisetime.tv_sec) * 1000000 + (curtime.tv_usec - alarmraisetime.tv_usec) << std::endl;

			if (((curtime.tv_sec - alarmraisetime.tv_sec) * 1000000) + (curtime.tv_usec - alarmraisetime.tv_usec) > wait_state_usec )
				iswait = 0; // reset wait period

		} else {

			// not in wait state

			if (diff < multiplicator_to_detect * diffavg)
				numthresholded = number_of_points_to_alarm; //reset thresholding count
			else {

				// debug
				// std::cout << "numthresholded: " << numthresholded << std::endl;

				// if number of subsequent points is enough, raise alarm
				if (--numthresholded == 0) {
					//	number of subsequent differences found

					// alarm raised
					isalarm = 1;
					alarmraisetime = curtime;
					iswait = 1;
					numthresholded = number_of_points_to_alarm;

					// pattern starts
					patternid++;
					ispattern = 1;
					patternraisetime = curtime;
				}
			}
		}

		// amend diffavg, use N_AMEND_AVGDIFF

		// do not amend if in wait state of detection sequence based on number_of_points_to_alarm
		if (iswait == 0 && numthresholded == number_of_points_to_alarm)
			diffavg = (diffavg * (n_amend_avgdiff - 1) + diff) / n_amend_avgdiff;

		// if < 1, set 1
		// if (diffavg < 1)
		//	diffavg = 1;

		// remember current value,
		lastval = curval;


		////////////////////////////////////////////////////////////////////////////////////////
		// output section
		// to output in production, manage to output variable isalarm (and possibly iswait)
		////////////////////////////////////////////////////////////////////////////////////////

		// output "lineid;timestamp;meas;diff;diffavg;isalarm;iswait"
		std::cout << lineid << ';' << p1 << ";";
		std::cout << curval << ";" << diffnoabs << ";" << diffavg << ";";
		std::cout << (numthresholded == number_of_points_to_alarm ? 0 : 1) << ";";
		std::cout << isalarm << ";";
		std::cout << iswait << ";" << (ispattern ? patternid : 0) << std::endl;

		////////////////////////////////////////////////////////////////////////////////////////
		// end of output section
		////////////////////////////////////////////////////////////////////////////////////////

	}

	return 0;
}

std::string trim(const std::string& str)
{
	size_t first = str.find_first_not_of(' ');
	if (std::string::npos == first)
	{
		return str;
	}
	size_t last = str.find_last_not_of(' ');
	return str.substr(first, (last - first + 1));
}






