window.initDimmerCubicInterpolation = function () {
    var ci = {
        channel: 0,
        chart: null,
        datapoints: [],
        config: null,
        modal_dialog: null,
        modal_dialog_button: null,
        changes: false,
        form: null,
        chart_colors: {
            red: 'rgb(255, 99, 132)',
            orange: 'rgb(255, 159, 64)',
            yellow: 'rgb(255, 205, 86)',
            green: 'rgb(75, 192, 192)',
            blue: 'rgb(54, 162, 235)',
            purple: 'rgb(153, 102, 255)',
            grey: 'rgb(201, 203, 207)'
        },
        load_data: function(channel) {
            var self = this;
            $.get('/dimmer-fw?type=read-ci&channel=' + channel, function(data) {
                // console.log(data);
                self.running(false);
                if (data.error) {
                    alert(data.error);
                    return;
                }
                self.changes = false;
                self.channel = channel;
                self.datapoints = data.data;
                var max_x = 0;
                for(var i = 0; i < self.datapoints.length; i++) {
                    if (self.datapoints[i].x > 0) {
                        max_x = self.datapoints[i].x;
                    }
                }
                if (max_x == 0) {
                    self.create_default_datapoints();
                }
                self.chart.data.datasets[0].label = 'Channel #' + (self.channel + 1);
                self.chart.data.datasets[0].data = self.datapoints;
                self.chart.update();
            }, 'json').fail(function() {
                alert('HTTP error while loading channel');
                self.running(false);
            });
        },
        store_data: function(channel, data) {
            var self = this;
            // console.log(channel, data);
            $.get('/dimmer-fw?type=write-ci&channel=' + channel + '&data='  + data, function(data) {
                // console.log(data);
                self.running(false);
                if (data.error) {
                    alert(data.error);
                    return;
                }
                self.changes = false;
            }, 'json').fail(function() {
                alert('HTTP error while storing channel');
                self.running(false);
            });
        },
        restore_defaults: function() {
            this.changes = false;
            this.create_default_datapoints();
            this.chart.data.datasets[0].data = this.datapoints;
            this.chart.update();
        },
        create_default_datapoints: function() {
            this.datapoints = [];
            for (i = 0.0; i <= 255.4; i += 255 / 7.0) {
                this.datapoints.push({ x: Math.round(i), y: Math.round(i) })
            }
        },
        // map_range: function (value, prec) {
        //     return Math.round(value * prec / 100.0);
        // },
        remove: function (arr, index) {
            var newArr = [];
            for (i = 0; i < arr.length; i++) {
                if (i != index) {
                    newArr.push(arr[i]);
                }
            }
            return newArr;
        },
        adjust_values: function(datasetIndex, index, value, end) {
            var do_update = false
            var dataset = this.chart.data.datasets[datasetIndex];
            var max_index = dataset.data.length - 1;
            if (index == 0) {
                if (value['x'] != 0) {
                    dataset.data[0]['x'] = 0;
                    do_update = true;
                }
            }
            else if (index == max_index) {
                if (value['x'] != 255) {
                    dataset.data[index]['x'] = 255;
                    do_update = true;
                }
            }
            else {
                minX = dataset.data[index - 1]['x'] + 3;
                maxX = dataset.data[index + 1]['x'] - 3;
                var x = dataset.data[index]['x'];
                if (x < minX) {
                    dataset.data[index]['x'] = minX;
                    do_update = true;
                }
                else if (x > maxX) {
                    dataset.data[index]['x'] = maxX;
                    do_update = true;
                }
            }
            if (end) {
                for (i = 0; i <= max_index; i++) {
                    v = dataset.data[index];
                    dataset.data[index] = { 'x': Math.round(v['x']), 'y': Math.round(v['y']) }
                }
            }
            if (do_update) {
                this.changes = true;
                this.chart.update(0);
            }
        },
        running: function(state) {
            form_set_disabled(this.form.find('button'), state);
            form_set_disabled(this.form.find('select'), state);
        },
        load_dataset: function (channel) {
            this.running(true);
            if (this.changes) {
                var self = this;
                self.open_modal(function() {
                    self.load_data(channel);
                }, 'Discard changes?');
            }
            else {
                this.load_data(channel);
            }
        },
        hex: function(n) {
            n = Math.round(n);
            s = n.toString(16);
            if (s.length == 1) {
                return '0' + s;
            }
            return s;
        },
        save_dataset: function(channel, data) {
            var str = '';
            for (var i = 0; i < data.length; i++) {
                v = data[i];
                str += this.hex(v.x) + this.hex(v.y);
            }
            this.store_data(channel, str)
        },
        open_modal: function(func, text) {
            this.modal_dialog.find('p').html(text);
            var self = this;
            this.modal_dialog_button.off('click').on('click', function () {
                self.modal_dialog.modal('hide');
                func();
            });
            this.modal_dialog.modal('show');
        },
        start: function() {
            Chart.defaults.global.elements.point.radius = 5;
            Chart.defaults.global.elements.point.hitRadius = 10;
            var self = this;
            this.config = {
                type: 'scatter',
                data: {
                    //labels: labels,
                    datasets: [{
                        label: 'Channel N/A',
                        data: this.datapoints,
                        showLine: true,
                        borderColor: this.chart_colors.red,
                        backgroundColor: 'rgba(0, 0, 0, 0)',
                        fill: false,
                        cubicInterpolationMode: 'monotone'
                    }],
                },
                options: {
                    onClick: function (e, n) {
                        if (n.length) {
                            var tmp = n[0];
                            var max_index = self.chart.data.datasets[tmp._datasetIndex].data.length - 1;
                            if (tmp._index > 0 && tmp._index < max_index) {
                                self.open_modal(function () {
                                    self.datapoints = self.remove(self.chart.data.datasets[tmp._datasetIndex].data, tmp._index);
                                    self.chart.data.datasets[tmp._datasetIndex].data = self.datapoints;
                                    self.chart.update();
                                }, 'Delete this point?');
                                $('body').click();
                            }
                        }
                    },
                    dragData: true,
                    dragX: true,
                    onDrag: function (e, datasetIndex, index, value) {
                        self.adjust_values(datasetIndex, index, value);
                        e.target.style.cursor = 'grabbing';
                    },
                    onDragEnd: function (e, datasetIndex, index, value) {
                        self.adjust_values(datasetIndex, index, value, true);
                        e.target.style.cursor = 'default';
                    },
                    spanGaps: true,
                    responsive: true,
                    maintainAspectRatio: false,
                    title: {
                        display: false,
                    },
                    tooltips: {
                        mode: 'index',
                        callbacks: {
                            label: function (tooltipItem, data) {
                                var x = Math.round(tooltipItem.xLabel * 100 / 255);
                                var y = Math.round(tooltipItem.yLabel * 100 / 255);
                                return 'Brightness ' + x + '% = Level ' + y + '%';
                            },
                        },
                    },
                    scales: {
                        xAxes: [{
                            display: true,
                            scaleLabel: {
                                display: true,
                                fontSize: 18,
                                labelString: 'Brightness in %'
                            },
                            ticks: {
                                callback: function (value) {
                                    return Math.round(value * 100 / 255);
                                },
                                min: 0,
                                max: 255,
                                stepSize: 255 / 20.0,
                            }
                        }],
                        yAxes: [{
                            display: true,
                            scaleLabel: {
                                display: true,
                                fontSize: 18,
                                labelString: 'Level in % (Half-wave on-time)'
                            },
                            ticks: {
                                callback: function (value) {
                                    return Math.round(value * 100 / 255);
                                },
                                min: 0,
                                max: 255,
                                stepSize: 255 / 20.0,
                            }
                        }]
                    }
                },
            };

            var ctx = document.getElementById('canvas').getContext('2d');
            this.chart = new Chart(ctx, this.config);

            this.modal_dialog = $('.modal');
            this.modal_dialog_button = this.modal_dialog.find('.btn-primary');
            this.form = $('#dimmer_ci');

            this.modal_dialog.on('hidden.bs.modal', function() {
                self.running(false);
            });
            $('#load').on('click', function () {
                self.load_dataset(parseInt($('#channel').val()))
            });
            $('#save').on('click', function () {
                self.running(true);
                self.save_dataset(self.channel, self.chart.data.datasets[0].data);
            });
            $('#default').on('click', function () {
                self.running(true);
                self.open_modal(function() {
                    self.restore_defaults();
                }, 'Discard changes?');
            });

            $('body').click();
            this.load_dataset(0);
        }
    };
    ci.start();
};
