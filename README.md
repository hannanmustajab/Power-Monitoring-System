# Power Monitoring System Using Non Invasive CT Sensors (SCT013)
[![IMAGE ALT TEXT HERE](https://inwfile.com/s-o/nmtz29.jpg)]()

Wi-Fi / Cellular Connected Power Monitoring System

This is a full implementation of Power Monitoring System using Particle 3rd Gen Boards (Argon, Boron). It uses a carrier board, and a CT breakout board. All CTs are connected to the CT breakout board, which is then connected to the carrier board. 

It uses dynamic sampling algorithm to minimize the number of data points and hence save data operations. There are cloud functions to change those parameters. 

It can be used in Three different operation modes, 
    - Single Phase Load
    - Three Phase Load ( Three Wires )
    - Three Phase Load ( Four Wires )

It stores all the data locally before sending it to the cloud in the buffer using PublishQueue library.
