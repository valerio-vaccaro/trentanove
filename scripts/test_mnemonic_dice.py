import numpy as np
import pandas as pd
from plotnine import *

NUM_VALUES = 2048
DICE_FACES = 6
MAX_LAUNCH = round(np.ceil(np.log(NUM_VALUES) / np.log(DICE_FACES)))


# initialize results structure
results = {}
for i in range(0, NUM_VALUES):
    results[i] = 0

def launch_dice(launch, prev_result):
    if (launch > MAX_LAUNCH):
        if prev_result < NUM_VALUES:
            results[prev_result] = results[prev_result] + 1
    else:
        for dice in range(1, DICE_FACES + 1):
            launch_dice(launch + 1, prev_result + (dice - 1) * (DICE_FACES**launch))

launch_dice(0, 0)

# Print values not found or duplicates
print('Duplicated or not found values:')
for i in range(0, NUM_VALUES):
    if not results[i] == 1:
        print(f'{i} - {results[i]}')

# convert to pandas dataframe
d = {'value': list(results.keys()), 'occurrences': list(results.values())}
df = pd.DataFrame(data=d)

# plot distribution
plt1 = ggplot(df, aes(x='value', y='occurrences')) + \
    geom_point() + \
    labs(title =f'Distribution 0-{NUM_VALUES-1} obtained launching {MAX_LAUNCH} times a dice with {DICE_FACES} faces.', x = 'Value', y = 'Occurrences')

ggsave(filename="distribution.png", plot=plt1, device='png', dpi=300)
