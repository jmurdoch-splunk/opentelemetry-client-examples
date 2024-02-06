import time
import logging
from random import randrange

################
# Main Program #
################

span_list = [ "first-child", "second-child", "third-child" ]

for span_name in span_list:
    # the value we want to change by per cycle
    delta_value = 0

    # the value we want to increment in a monotonically non-decreasing function
    counter_value = 0

    logging.info('Starting ' + span_name)
    while counter_value < 50:
        # create a new random value
        delta_value = randrange(10)

        # increment the counter with the random value
        counter_value += delta_value

        # print the values
        print("Span:", span_name, "\tDelta:", delta_value, "\tCounter:", counter_value)

        # wait a second
        time.sleep(1)
    logging.info('Completed ' + span_name)
