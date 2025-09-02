import { AppBar, Toolbar, Typography, IconButton, Box, Card, CardContent, Grid, Button, LinearProgress, Drawer, List, ListItem, ListItemButton, ListItemIcon, ListItemText, Snackbar, Alert } from '@mui/material';
import MenuIcon from '@mui/icons-material/Menu';
import DashboardIcon from '@mui/icons-material/Dashboard';
import SettingsIcon from '@mui/icons-material/Settings';
import './App.css';
import { useEffect, useState, useRef } from 'react';
import { connectWebSocket, disconnectWebSocket, sendWebSocketMessage, type IoTData, type WebSocketIncomingMessage, type ErrorMessage } from './utils/sensorApi';
import EchartsReact from 'echarts-for-react';
import { BrowserRouter as Router, Routes, Route, Link } from 'react-router';
import ZoneManagement from './pages/ZoneManagement';
import { getZones } from './utils/zoneApi';
import { type Zone } from './utils/sensorApi';

const MAX_HISTORY_POINTS = 500; // Limit historical data points

function App() {
  const [iotData, setIotData] = useState<IoTData | null>(null);
  const [snackbarOpen, setSnackbarOpen] = useState<boolean>(false);
  const [snackbarMessage, setSnackbarMessage] = useState<string>('');
  const [snackbarSeverity, setSnackbarSeverity] = useState<'success' | 'error' | 'info' | 'warning'>('info');
  const historicalDataRef = useRef<IoTData[]>([]);
  const [, setHistoricalDataTrigger] = useState(0);
  const [drawerOpen, setDrawerOpen] = useState(false);
  const [zones, setZones] = useState<Zone[]>([]);

  const flowChartRef = useRef<EchartsReact | null>(null);
  const moistureChartRef = useRef<EchartsReact | null>(null);

  const toggleDrawer = (open: boolean) => (event: React.KeyboardEvent | React.MouseEvent) => {
    if (
      event.type === 'keydown' &&
      ((event as React.KeyboardEvent).key === 'Tab' ||
        (event as React.KeyboardEvent).key === 'Shift')
    ) {
      return;
    }
    setDrawerOpen(open);
  };

  const flowChartOptions = {
    title: {
      text: 'Flow Meter Data',
      left: 'center',
    },
    tooltip: {
      trigger: 'axis',
    },
    xAxis: {
      type: 'category',
      data: historicalDataRef.current.map(data => new Date(data.time).toLocaleTimeString()),
    },
    yAxis: {
      type: 'value',
      name: 'Flow Volume (L)',
    },
    series: [
      {
        name: 'Flow 1',
        type: 'line',
        data: historicalDataRef.current.map(data => data.volumes[0]),
      },
      {
        name: 'Flow 2',
        type: 'line',
        data: historicalDataRef.current.map(data => data.volumes[1]),
      },
      {
        name: 'Flow 3',
        type: 'line',
        data: historicalDataRef.current.map(data => data.volumes[2]),
      },
    ],
  };

  const moistureChartOptions = {
    title: {
      text: 'Moisture Sensor Data',
      left: 'center',
    },
    tooltip: {
      trigger: 'axis',
    },
    xAxis: {
      type: 'category',
      data: historicalDataRef.current.map(data => new Date(data.time).toLocaleTimeString()),
    },
    yAxis: {
      type: 'value',
      name: 'Moisture Level',
    },
    series: iotData?.moists.map((_, index) => ({
      name: `Moisture ${index + 1}`,
      type: 'line',
      data: historicalDataRef.current.map(data => data.moists[index]),
    })) || [],
  };

  useEffect(() => {
    connectWebSocket((message: WebSocketIncomingMessage) => {
      if (message.type === "state") {
        setIotData(message);
        historicalDataRef.current = [...historicalDataRef.current, message].slice(-MAX_HISTORY_POINTS);
        setHistoricalDataTrigger(prev => prev + 1); // Trigger re-render
      } else if (message.type === "command_ack") {
        // Ignore the very first command_ack after connection
        setSnackbarOpen(true);
        setSnackbarMessage(`Command ACK for: ${message.command}`);
        setSnackbarSeverity('info');
        // Optionally clear the ack message after some time
        setTimeout(() => setSnackbarOpen(false), 5000);
      }
    }, (error: Event | ErrorMessage) => {
      console.error('WebSocket connection error:', error);
      setSnackbarOpen(true);
      if ((error as ErrorMessage).type === "error") {
        setSnackbarMessage(`WebSocket Error: ${(error as ErrorMessage).message}`);
      } else {
        setSnackbarMessage(`WebSocket Connection Error: ${error.type}`);
      }
      setSnackbarSeverity('error');
      setTimeout(() => setSnackbarOpen(false), 5000);
    });

    return () => {
      disconnectWebSocket();
    };
  }, []);

  const fetchZonesData = async () => {
    try {
      const zonesData = await getZones();
      setZones(zonesData);
    } catch (error) {
      console.error("Error fetching zones:", error);
    }
  };

  useEffect(() => {
    fetchZonesData();
  }, []);

  useEffect(() => {
    const handleResize = () => {
      flowChartRef.current?.getEchartsInstance()?.resize();
      moistureChartRef.current?.getEchartsInstance()?.resize();
    };

    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
    };
  }, []);

  const handleValveControl = (valve: number, newState: boolean) => {
    if (newState) {
      sendWebSocketMessage({ deviceId: "IR_CONTROLLER_01", command: "OPEN_VALVE", valve: valve });
    } else {
      sendWebSocketMessage({ deviceId: "IR_CONTROLLER_01", command: "CLOSE_VALVE", valve: valve });
    }
  };

  return (
    <Router>
      <Box sx={{ flexGrow: 1 }}>
        <AppBar position="fixed" sx={{ width: '100%' }}>
          <Toolbar>
            <IconButton
              size="large"
              edge="start"
              color="inherit"
              aria-label="open drawer"
              sx={{ mr: 2 }}
              onClick={toggleDrawer(true)}
            >
              <MenuIcon />
            </IconButton>
            <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
              Smart Irrigation
            </Typography>
          </Toolbar>
        </AppBar>

        <Drawer anchor="left" open={drawerOpen} onClose={toggleDrawer(false)}>
          <Box
            sx={{ width: 250 }}
            role="presentation"
            onClick={toggleDrawer(false)}
            onKeyDown={toggleDrawer(false)}
          >
            <List>
              <ListItem disablePadding>
                <ListItemButton component={Link} to="/">
                  <ListItemIcon>
                    <DashboardIcon />
                  </ListItemIcon>
                  <ListItemText primary="Dashboard" />
                </ListItemButton>
              </ListItem>
              <ListItem disablePadding>
                <ListItemButton component={Link} to="/zones">
                  <ListItemIcon>
                    <SettingsIcon />
                  </ListItemIcon>
                  <ListItemText primary="Zone Management" />
                </ListItemButton>
              </ListItem>
            </List>
          </Box>
        </Drawer>

        <Box component="main" sx={{ p: 3, mt: 8 }}> 
          <Routes>
            <Route path="/" element={(
              <Grid container spacing={3}>
                {[...zones].sort((a, b) => a.displayPosition - b.displayPosition).map((zone) => (
                  <Grid size={{ xs: 12, sm:12, md: 6, lg: 4 }} key={zone.id}>
                    <Card>
                      <CardContent>
                        <Typography variant="h5" component="div">
                          Zone: {zone.name} (Valve {zone.valveNumber})
                        </Typography>
                        <Typography sx={{ mb: 1.5 }} color="text.secondary">
                          Current Readings
                        </Typography>
                        {/* Flowmeter Visualization */}
                        {iotData && zone.flowmeter !== undefined && iotData.flows[zone.flowmeter] !== undefined && ( // Check if flowmeter data exists
                          <Typography variant="body2" sx={{ mt: 1 }}>
                            Flow Volume {zone.flowmeter + 1}: {iotData.volumes[zone.flowmeter]?.toFixed(2)} L / {zone.wateringRequirementLiters} L
                            <LinearProgress
                              variant="determinate"
                              value={((iotData.volumes[zone.flowmeter] || 0) / (zone.wateringRequirementLiters || 100)) * 100}
                              sx={{ height: 10, borderRadius: 5 }}
                            />
                          </Typography>
                        )}

                        {/* Moisture Sensors Display */}
                        {iotData && zone.moistureSensors?.length > 0 && (
                          <Box sx={{ mt: 2 }}>
                            {zone.moistureSensors.map((sensorIndex) => (
                              <Typography variant="body2" sx={{ mt: 1 }} key={`moist-${zone.id}-${sensorIndex}`}>
                                Moisture {sensorIndex + 1}: {iotData.moists[sensorIndex]}<br />
                              </Typography>
                            ))}
                          </Box>
                        )}

                        {/* Valve Control Buttons */}
                        <Typography variant="h5" component="div" sx={{ mt: 2 }}>
                          Irrigation Control
                        </Typography>
                        <Typography sx={{ mb: 1.5 }} color="text.secondary">
                          Valves Status
                        </Typography>
                        {iotData?.valves[zone.valveNumber] !== undefined && (
                          <Box sx={{ mb: 1 }}>
                            <Typography variant="body2">Valve {zone.valveNumber}: {iotData.valves[zone.valveNumber] ? 'Open' : 'Closed'}</Typography>
                            <Button
                              variant="contained"
                              color="primary"
                              sx={{ mr: 1 }}
                              onClick={() => handleValveControl(zone.valveNumber, true)}
                              disabled={!!iotData.valves[zone.valveNumber]}
                            >
                              Open Valve {zone.valveNumber}
                            </Button>
                            <Button
                              variant="outlined"
                              color="secondary"
                              onClick={() => handleValveControl(zone.valveNumber, false)}
                              disabled={!iotData.valves[zone.valveNumber]}
                            >
                              Close Valve {zone.valveNumber}
                            </Button>
                          </Box>
                        )}
                        {/* commandAck Snackbar will be rendered at the root of the dashboard */}
                      </CardContent>
                    </Card>
                  </Grid>
                ))}

                {/* Charts & Analytics Card (Remains Separate) */}
                <Grid size={{ xs: 12, sm: 12, md: 12, lg: 12 }}>
                  <Card>
                    <CardContent>
                      <Typography variant="h5" component="div">
                        Charts & Analytics
                      </Typography>
                      <Typography sx={{ mb: 1.5 }} color="text.secondary">
                        Data Visualization
                      </Typography>
                      {historicalDataRef.current.length > 0 ? (
                        <>
                          <EchartsReact
                            ref={flowChartRef}
                            option={flowChartOptions}
                            style={{ height: '300px', width: '100%', marginBottom: '20px' }}
                          />
                          <EchartsReact
                            ref={moistureChartRef}
                            option={moistureChartOptions}
                            style={{ height: '300px', width: '100%' }}
                          />
                        </>
                      ) : (
                        <Typography variant="body2">Loading chart data...</Typography>
                      )}
                      <Typography variant="body2" sx={{ mt: 2 }}>
                        Last Update: {iotData?.time ? new Date(iotData.time).toLocaleTimeString() : 'N/A'}
                      </Typography>
                    </CardContent>
                  </Card>
                </Grid>
              </Grid>
            )} />
            <Route path="/zones" element={<ZoneManagement onZonesUpdate={fetchZonesData} />} />
          </Routes>
          <Snackbar open={snackbarOpen} autoHideDuration={5000} onClose={() => setSnackbarOpen(false)}>
            <Alert onClose={() => setSnackbarOpen(false)} severity={snackbarSeverity} sx={{ width: '100%' }}>
              {snackbarMessage}
            </Alert>
          </Snackbar>
        </Box>
      </Box>
    </Router>
  );
}

export default App;
