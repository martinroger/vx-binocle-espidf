function setup() {
    can.setFilter(0x780, 0x7F0, 0);
}

function gotCANFrame(bus, id, len, data) {
    var respData = [];
    respData[0] = 0x30;
    respData[1] = 0xFF;
    respData[2] = 0x00; // One ms STMin
    if (len == 8) {
        if (data[0] == 0x10) {
            if (id == 0x784) {
                can.sendFrame(0, 0x785, 8, respData);
                host.log("Sent RDB FC");
            }
            if (id == 0x782) {
                can.sendFrame(0, 0x783, 8, respData);
                host.log("Sent LDB FC");
            }
        }
    }
}