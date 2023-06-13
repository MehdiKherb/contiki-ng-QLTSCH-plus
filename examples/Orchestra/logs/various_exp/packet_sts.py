import sys
from pandas import DataFrame
import pandas as pd

# INPUT_FILE = sys.argv[1]

# delays = {}
# pkt_sent = {}
# pkt_received = {}


def main(file_name):
    delays = {}
    pkt_sent = {}
    pkt_received = {}

    with open(file_name, "r") as f:
        for line in f:
            if not (("INFO: App" in line) and ("Sent_to" in line or "Received_from" in line)):
                continue
            if "Sent_to" in line:
                fields = line.split()
                node_id = int(fields[1][3:])
                if node_id in pkt_sent:
                    pkt_sent[node_id][int(fields[-1])] = float(fields[0])
                else:
                    pkt_sent[node_id] = {}
                    pkt_sent[node_id][int(fields[-1])] = float(fields[0])
            elif "Received_from" in line:
                fields = line.split()
                node_id = int(fields[6])
                if node_id in pkt_received:
                    pkt_received[node_id][int(fields[-1])] = float(fields[0])
                else:
                    pkt_received[node_id] = {}
                    pkt_received[node_id][int(fields[-1])] = float(fields[0])
            else:
                print("Error: line not recognized")

    for node_id in pkt_sent.keys():
        if node_id in pkt_received:
            pkt_id_list = sorted(set(pkt_sent[node_id].keys()) & set(
                pkt_received[node_id].keys()))
            not_received = sorted(
                set(pkt_sent[node_id].keys()) - set(pkt_received[node_id].keys()))
            pdr = float(len(pkt_received[node_id]))/len(pkt_sent[node_id])*100
            dels = []
            for pkt_id in pkt_id_list:
                dels.append((pkt_received[node_id]
                            [pkt_id] - pkt_sent[node_id][pkt_id])/1000000)
            delays[node_id] = (dels, not_received, pdr)
            for d in dels:
                if d < 0:
                    print("Negative:", d)

        else:
            delays[node_id] = ([], sorted(pkt_sent[node_id].keys()), 0.0)

    total_time = 0
    counter = 0
    average_pdr = 0
    for i in delays.keys():
        total_time += sum(delays[i][0])
        counter += len(delays[i][0])
        average_pdr += delays[i][2]
    average_pdr = average_pdr/len(delays.keys())

    # special traffic ==============================================================================
    s50_total_time = 0
    s50_counter = 0
    s50_average_pdr = 0
    s50_nodes = 0

    s10_total_time = 0
    s10_counter = 0
    s10_average_pdr = 0
    s10_nodes = 0

    for i in delays.keys():
        if i in [2, 9, 58, 65]:
            s50_total_time += sum(delays[i][0])
            s50_counter += len(delays[i][0])
            s50_average_pdr += delays[i][2]
            s50_nodes += 1
        else:
            s10_total_time += sum(delays[i][0])
            s10_counter += len(delays[i][0])
            s10_average_pdr += delays[i][2]
            s10_nodes += 1

    s50_average_pdr = s50_average_pdr/s50_nodes
    s10_average_pdr = s10_average_pdr/s10_nodes

    # special traffic ==============================================================================

    total_bytes = 0
    for i in pkt_received.keys():
        total_bytes += len(pkt_received[i])
    total_bytes = (total_bytes * 50)/1024
    print("\n*********** Statistics for Network *************")
    print("Average Delay: {:.2f} ms".format(total_time/counter))
    print("Packet Delivery Ratio: {:.2f}%".format(average_pdr))
    print("Total Bytes Received: {:.2f} kB".format(total_bytes))

    # print("\n********* Average Delays for each Node *********")
    # for i in sorted(delays.keys()):
    #     if len(delays[i][0]) != 0:
    #         print("Node {}: {:.2f} ms".format(
    #             i, sum(delays[i][0])/len(delays[i][0])))
    #     else:
    #         print("Node {}: No packet received -> No Delay".format(i))
    #     print("Packet IDs not received: {}".format(delays[i][1]))
    #     print("Packet Delivery Ratio: {:.2f}%".format(delays[i][2]))
    #     print("Total sent packets:", len(pkt_sent[i]))
    #     print('-' * 50)
    print("Number of Active Nodes:", len(pkt_sent))
    # print("Active nodes:", sorted(pkt_sent.keys()))
    return [s50_total_time/s50_counter, s50_average_pdr, s10_total_time/s10_counter, s10_average_pdr, total_time/counter, average_pdr, total_bytes, len(pkt_sent)]


if __name__ == "__main__":
    main_list = []
    protocol = sys.argv[1]
    for i in range(1, 2):
        file_name = protocol + "-" + "27354" + ".txt"
        print('\n', "@"*20, "File Number", i, "@"*20)
        main_list.append(main(file_name))
    print("\n\n", "@"*20, "Final Results", "@"*20)

    df3 = DataFrame({'Delay_50B': [main_list[x][0] for x in range(1)], 'PDR_50B': [main_list[x][1] for x in range(1)], 'Delay_10B': [main_list[x][2] for x in range(1)], 'PDR_10B': [main_list[x][3] for x in range(1)], 'Delay_Network': [main_list[x][4] for x in range(1)], 'PDR_network': [main_list[x][5] for x in range(
        1)], 'TotalData_KB': [main_list[x][6] for x in range(1)], 'Active_Nodes': [main_list[x][7] for x in range(1)]})

    # Create a Pandas Excel writer using XlsxWriter as the engine.
    writer = pd.ExcelWriter(protocol + '.xlsx', engine='xlsxwriter')

    # Write each dataframe to a different worksheet.
    df3.to_excel(writer, sheet_name=protocol)

    # Close the Pandas Excel writer and output the Excel file.
    writer.save()
    print("\n\n", "@"*20, "Finished", "@"*20)
