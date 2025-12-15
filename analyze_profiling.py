#!/usr/bin/env python3

import argparse
import pandas as pd
import plotly.express as px


def analyze_profiling_data(data, output_hist_json, output_hist_png, output_box_json,
                           output_box_png):

    # Extract the durations column (only one column exists)
    durations = data.iloc[:, 0]

    # Compute basic statistics
    mean_duration = durations.mean()
    median_duration = durations.median()
    min_duration = durations.min()
    max_duration = durations.max()
    std_duration = durations.std()

    # Print statistics
    print("Basic Statistics of Runtime Durations:")
    print(f"Mean: {mean_duration:.2f} µs")
    print(f"Median: {median_duration:.2f} µs")
    print(f"Min: {min_duration} µs")
    print(f"Max: {max_duration} µs")
    print(f"Standard Deviation: {std_duration:.2f} µs")

    # Create histogram
    hist_fig = px.histogram(durations, nbins=100, labels={'value': 'Duration (µs)'}, title='Histogram of Runtime Durations')

    # Create boxplot
    box_fig = px.box(durations, labels={'value': 'Duration (µs)'}, title='Boxplot of Runtime Durations')

    # Save plots
    hist_fig.write_json(output_hist_json)
    hist_fig.write_image(output_hist_png)
    box_fig.write_json(output_box_json)
    box_fig.write_image(output_box_png)


def main():
    argparser = argparse.ArgumentParser(description="Analyze profiling data from CSV file.")
    argparser.add_argument(
        "--input_csv",
        type=str,
        default="192.168.178.83_30004_parse_durations.csv",
        help="Path to the input CSV file containing profiling data.",
    )
    argparser.add_argument(
        "--output_hist_json",
        type=str,
        default="runtime_histogram.json",
        help="Path to save the histogram JSON file.",
    )
    argparser.add_argument(
        "--output_hist_png",
        type=str,
        default="runtime_histogram.png",
        help="Path to save the histogram PNG file.",
    )
    argparser.add_argument(
        "--output_box_json",
        type=str,
        default="runtime_boxplot.json",
        help="Path to save the boxplot JSON file.",
    )
    argparser.add_argument(
        "--output_box_png",
        type=str,
        default="runtime_boxplot.png",
        help="Path to save the boxplot PNG file.",
    )
    args = argparser.parse_args()


    # Load the CSV file
    df = pd.read_csv(args.input_csv, header=0)

    analyze_profiling_data(df, args.output_hist_json, args.output_hist_png, 
                           args.output_box_json, args.output_box_png)


if __name__ == "__main__":
    main()
