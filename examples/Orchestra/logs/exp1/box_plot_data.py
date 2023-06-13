import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sys

metrics = ['Delay_50B', 'PDR_50B', 'Delay_10B', 'PDR_10B',
           'Delay_Network', 'PDR_network', 'TotalData_KB', 'Active_Nodes']


#pts = ['Orchestra', 'OST', 'ALICE_rx_sf7', 'ALICE_sf7', 'ALICE_rx_sf19',
#       'ALICE_sf19', 'MSF', 'QL-TSCH_5', 'QL-TSCH_9', 'QL-TSCH_15']
pts = [ 'QL-TSCH-plus', 'QL-TSCH']

def read_file(file_name):
    dfs = pd.read_excel(file_name, sheet_name=None)
    ready_df = {}
    for metric in metrics:
        tmp_df = {}
        for protocol in pts:
            tmp_df[protocol] = dfs[protocol][metric]
        ready_df[metric] = tmp_df

    # # Create a Pandas Excel writer using XlsxWriter as the engine.
    # writer = pd.ExcelWriter('Ready_to_plot.xlsx', engine='xlsxwriter')
    # for metric in metrics:
    #     df = pd.DataFrame(ready_df[metric])
    #     df.to_excel(writer, sheet_name=metric)

    # # Close the Pandas Excel writer and output the Excel file.
    # writer.save()

    # plot the data
    dfs = ready_df  # pd.read_excel('Ready_to_plot.xlsx', sheet_name=None)
    for key in metrics:
        all_data = [dfs[key][x] for x in pts]
        labels = pts

        num_boxes = len(all_data)
        medians = [sum(all_data[i])/len(all_data[i]) for i in range(num_boxes)]

        fig, ax = plt.subplots(figsize=(5, 4))

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
        if 'Delay' in key:
            plt.ylim(0, 1)

        if 'Active Nodes' in key:
            plt.ylim(60, 65)

        if 'PDR' in key and 'Traffic' in key:
            plt.ylim(90, 105)

        # fill with colors
        #colors = ['pink', 'blue', 'green', 'red', 'black',
        #          'orange', 'purple', 'tan', 'brown', 'grey']
        colors = ['green', 'blue']
        for patch, color in zip(bplot['boxes'], colors):
            patch.set_facecolor(color)

        # put average values above the figures
        pos = np.arange(num_boxes) + 1
        upper_labels = [str(round(s, 2)) for s in medians]
        weights = ['bold', 'semibold']
        for tick, label in zip(range(num_boxes), ax.get_xticklabels()):
            k = tick % 2
            ax.text(pos[tick], .95, upper_labels[tick],
                    transform=ax.get_xaxis_transform(),
                    horizontalalignment='center', size='x-small',
                    weight=weights[k], color=colors[tick])

        # adding horizontal grid lines
        ax.grid(True)
        #if key == 'Throughput':
        #    ax.set_title('Average Throughput')
        #else:
        ax.set_title(key)

        plt.show()


if __name__ == '__main__':
    file_name = sys.argv[1]
    read_file(file_name)
