function initSmallScope(canvas) {
    var chart = new SmoothieChart({
            responsive: true,
            millisPerPixel: 71,
            grid: {strokeStyle: 'rgba(119,119,119,0.28)', verticalSections: 7},
            tooltip: true,
            tooltipLine: {strokeStyle: '#bbbbbb'}
        }),
        series = new TimeSeries();

    chart.addTimeSeries(series, {lineWidth: 2, strokeStyle: '#ff0000'});
    chart.streamTo(canvas, 500);
}