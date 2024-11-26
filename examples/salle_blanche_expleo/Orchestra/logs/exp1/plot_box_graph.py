import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import sys


def read_file(file_name):
    pts = ['QL-TSCH-plus']
    dfs = pd.read_excel(file_name, sheet_name=None)
    for key in dfs.keys():
        all_data = [dfs[key][x] for x in pts]
        labels = pts
        num_boxes = len(all_data)
        medians = [sum(all_data[i])/len(all_data[i]) for i in range(num_boxes)]

        fig, ax = plt.subplots(figsize=(9, 6))

        bplot = ax.boxplot(all_data,
                           vert=True,  # vertical box alignment
                           showfliers=False,
                           showmeans=True,
                           meanline=True,
                           patch_artist=True,  # fill with color
                           labels=labels)  # will be used to label x-ticks

        # Set the axes ranges and axes labels
        ax.set_xticklabels(pts, rotation=45, fontsize=8)
        fig.subplots_adjust(left=0.086, right=0.97, top=0.943, bottom=0.183)

        # fill with colors
        #colors = ['pink', 'blue', 'green', 'red', 'black',
        #          'orange', 'purple', 'tan', 'brown', 'grey']
        colors = ['green']
        for patch, color in zip(bplot['boxes'], colors):
            patch.set_facecolor(color)

        # put average values above the figures
        pos = np.arange(num_boxes) + 1
        upper_labels = [str(round(s, 1)) for s in medians]
        weights = ['bold', 'semibold']
        for tick, label in zip(range(num_boxes), ax.get_xticklabels()):
            k = tick % 2
            ax.text(pos[tick], .95, upper_labels[tick],
                    transform=ax.get_xaxis_transform(),
                    horizontalalignment='center', size='small',
                    weight=weights[k], color=colors[tick])

        # adding horizontal grid lines
        ax.grid(True)
        ax.set_ylabel('Energy (Joule)')
        ax.set_title(key, fontsize=14)

        plt.show()


if __name__ == '__main__':
    file_name = sys.argv[1]
    read_file(file_name)
