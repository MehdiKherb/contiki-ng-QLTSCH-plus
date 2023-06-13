from pandas import DataFrame
import pandas as pd
import sys

# INPUT_FILE = sys.argv[1]
# From Sky node datasheet

CURRENT_MA = {
    "cpu": 2.4,
    "LPM": 1.2,
    "Deep LPM": 0,
    "rx": 21.8,
    "tx": 19.5,
}

STATES = list(CURRENT_MA.keys())

VOLTAGE = 3.0  # assume 3 volt batteries
RTIMER_ARCH_SECOND = 1000000

files = {}


def main(file_name):
    seq_numbers = {}
    with open(file_name, "r") as f:
        for line in f:
            if "[INFO: Energest  ] seq_num" not in line:
                continue
            fields = line.split()
            seq_num = int(fields[5][8:])

            if seq_num in seq_numbers:
                avr_current = CURRENT_MA['tx'] * \
                    int(fields[6][3:]) / int(fields[-1][5:])
                seq_numbers[seq_num]['tx'] += avr_current * \
                    VOLTAGE  # * 10 ---> to make it in Jouls
                seq_numbers[seq_num]['radio'] += avr_current * VOLTAGE

                avr_current = CURRENT_MA['rx'] * \
                    int(fields[7][3:]) / int(fields[-1][5:])
                seq_numbers[seq_num]['rx'] += avr_current * \
                    VOLTAGE  # * 10 ---> to make it in Jouls
                seq_numbers[seq_num]['radio'] += avr_current * VOLTAGE

                avr_current = CURRENT_MA['cpu'] * \
                    int(fields[-1][5:]) / int(fields[-1][5:])
                seq_numbers[seq_num]['cpu'] += avr_current * \
                    VOLTAGE  # * 10 ---> to make it in Jouls
            else:
                seq_numbers[seq_num] = {}
                avr_current = CURRENT_MA['tx'] * \
                    int(fields[6][3:]) / int(fields[-1][5:])
                seq_numbers[seq_num]['tx'] = avr_current * \
                    VOLTAGE  # * 10 ---> to make it in Jouls
                seq_numbers[seq_num]['radio'] = avr_current * VOLTAGE

                avr_current = CURRENT_MA['rx'] * \
                    int(fields[7][3:]) / int(fields[-1][5:])
                seq_numbers[seq_num]['rx'] = avr_current * \
                    VOLTAGE  # * 10 ---> to make it in Jouls
                seq_numbers[seq_num]['radio'] += avr_current * VOLTAGE

                avr_current = CURRENT_MA['cpu'] * \
                    int(fields[-1][5:]) / int(fields[-1][5:])
                seq_numbers[seq_num]['cpu'] = avr_current * \
                    VOLTAGE  # * 10 ---> to make it in Jouls

    files[file_name] = seq_numbers


def save_as_xlsx(protocol):
    ready_data = {}
    for i in range(1, 359):
        ready_data[str(i)] = {}
        for j in range(1, 2):
            key = protocol + "-" + "27354" + ".txt"
            ready_data[str(i)][j] = files[key][i]['radio']
    df1 = DataFrame(ready_data)

    ready_data = {}
    for i in range(1, 359):
        ready_data[str(i)] = {}
        for j in range(1, 2):
            key = protocol + "-" + "27354" + ".txt"
            ready_data[str(i)][j] = files[key][i]['rx']
    df2 = DataFrame(ready_data)

    ready_data = {}
    for i in range(1, 359):
        ready_data[str(i)] = {}
        for j in range(1, 2):
            key = protocol + "-" + "27354" + ".txt"
            ready_data[str(i)][j] = files[key][i]['tx']
    df3 = DataFrame(ready_data)

    # Create a Pandas Excel writer using XlsxWriter as the engine.
    writer = pd.ExcelWriter('Energy-watts.xlsx', engine='xlsxwriter')

    # Write each dataframe to a different worksheet.
    sorted_columns = sorted(df1.columns, key=int)
    df1_sorted = df1[sorted_columns]

    # Write the sorted DataFrame to the Excel sheet
    df1_sorted.to_excel(writer, sheet_name='Radio_total')
    
    # Write each dataframe to a different worksheet.
    sorted_columns = sorted(df2.columns, key=int)
    df2_sorted = df2[sorted_columns]

    # Write the sorted DataFrame to the Excel sheet
    df2_sorted.to_excel(writer, sheet_name='Rx_Energy')

    # Write each dataframe to a different worksheet.
    sorted_columns = sorted(df3.columns, key=int)
    df3_sorted = df3[sorted_columns]

    # Write the sorted DataFrame to the Excel sheet
    df3_sorted.to_excel(writer, sheet_name='Tx_Energy')

    # Close the Pandas Excel writer and output the Excel file.
    writer.save()


if __name__ == "__main__":
    protocol = sys.argv[1]
    for i in range(1, 2):
        file_name = protocol + "-" + "27354" + ".txt"
        print('\n', "@"*20, "File Number", i, "@"*20)
        main(file_name)
    save_as_xlsx(protocol)
    print('\n', "@"*20, "Finished", "@"*20)
