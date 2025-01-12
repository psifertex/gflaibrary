let eventLoop = require("event_loop");
let gui = require("gui");
let submenuView = require("gui/submenu");
let dialogView = require("gui/dialog");
let storage = require("storage");
let subghz = require("subghz");
let Math = require("math");

let lastMenu = null;

/* TODO: 
    * Finish converting the logic from generate.py into working JS here so it actually works!
    * Add more customization for the colors 
    * Convert to real C app maybe? 
    * Add flashing patterns based on the official handheld remote (current menus map to that)
*/

function ColorCommand(red, green, blue, brightness, delay_time) {
    this.red = red;
    this.green = green;
    this.blue = blue;
    this.brightness = brightness || 15;
    this.delay_time = delay_time || 0;

    this.init = function() {
        if (this.red < 0 || this.red > 15 || this.green < 0 || this.green > 15 || this.blue < 0 || this.blue > 15 || this.brightness < 0 || this.brightness > 15 || this.delay_time < 0 || this.delay_time > 0xffffffff) {
            console.error("Invalid color or brightness value");
            return;
        }
        this.red = Math.floor(this.red * (this.brightness / 15.0));
        this.green = Math.floor(this.green * (this.brightness / 15.0));
        this.blue = Math.floor(this.blue * (this.brightness / 15.0));
    };

    this.calcChecksum = function() {
        let checksum_nibble = 5;
        return ((this.red ^ this.green ^ this.blue ^ checksum_nibble) << 4) | checksum_nibble;
    };

    this.encodeBody = function() {
        let body = Uint8Array(6);
        body[0] = 0xaa;
        body[1] = 0xff;
        body[2] = this.red << 4;
        body[3] = this.green << 4;
        body[4] = this.blue << 4;
        body[5] = this.calcChecksum();
        return body;
    };

    this.encode = function() {
        let start_delay = Uint8Array(0);
        if (this.delay_time) {
            start_delay = Uint8Array(6);
            start_delay[0] = 0xcd;
            let delay_bytes = Uint8Array(Uint32Array([this.delay_time]).buffer);
            start_delay.set(delay_bytes, 1);
            start_delay[5] = 0xcd;
        }
        let body = this.encodeBody();
        let encoded = Uint8Array(start_delay.length + body.length + 1);
        encoded.set(start_delay);
        encoded.set(body, start_delay.length);
        encoded[start_delay.length + body.length] = 0xcc;
        return encoded;
    };

    this.init();
}

function ColorCommands(colors) {
    this.colors = colors;

    this.encode = function() {
        let header = Uint8Array([0x50, 0xcc]);
        let encodedColors = this.colors.map(function(color) {
            return color.encode();
        });
        let encoded = Uint8Array(header.length + encodedColors.reduce(function(acc, val) {
            return acc + val.length;
        }, 0));
        encoded.set(header);
        let offset = header.length;
        encodedColors.forEach(function(color) {
            encoded.set(color, offset);
            offset += color.length;
        });
        return encoded;
    };
}

function fixedColor(red, green, blue, brightness, delay_time) {
	let colors = [];
	colors.push(ColorCommand(red, green, blue, brightness, delay_time));
	let colorCommands = ColorCommands(colors);
	let encoded = colorCommands.encode();
	print(arraybuf_to_string(encoded.buffer));
	return arraybuf_to_string(encoded.buffer);
}

function generateContent(selection) {
    if (selection === "red") {
		return fixedColor(15, 0, 0, 15, 250);
    } else if (selection === "blue") {
		return fixedColor(0, 0, 15, 15, 250);
    } else if (selection === "green") {
		return fixedColor(0, 15, 0, 15, 250);
    } else if (selection === "violet") {
		return fixedColor(15, 0, 15, 15, 250);
    } else if (selection === "yellow") {
		return fixedColor(15, 15, 0, 15, 250);
    } else if (selection === "cyan") {
		return fixedColor(0, 15, 15, 15, 250);
    } else if (selection === "white") {
		return fixedColor(15, 15, 15, 15, 250);
    } else {
        console.error("Invalid color selection");
        return "";
    }
}

function arraybuf_to_string(buffer) {
    let str = "";
    let view = Uint8Array(buffer);
    for (let i = 0; i < view.length; i++) {
        str += String.fromCharCode(view[i]);
    }
    return str;
}


let views = {
    main: submenuView.makeWith({
        header: "Main Menu",
        items: ["colors", "patterns", "off"],
    }),
    colors: submenuView.makeWith({
        header: "Colors",
        items: ["red", "blue", "green", "violet", "yellow", "cyan", "white"],
    }),
    patterns: submenuView.makeWith({
        header: "Patterns",
        items: ["flash", "crossfade", "spark", "auto"],
    }),
    dialog: dialogView.make(),
};

eventLoop.subscribe(views.main.chosen, function(_sub, index, gui, eventLoop, views) {
    if (index === 0) {
        gui.viewDispatcher.switchTo(views.colors);
    } else if (index === 1) {
        gui.viewDispatcher.switchTo(views.patterns);
    } else if (index === 2) {
        eventLoop.stop();
    }
}, gui, eventLoop, views);

eventLoop.subscribe(views.colors.chosen, function(_sub, index, gui, views) {
    let selection;
    if (index === 0) {
        selection = "red";
    } else if (index === 1) {
        selection = "blue";
    } else if (index === 2) {
        selection = "green";
    } else if (index === 3) {
        selection = "violet";
    } else if (index === 4) {
        selection = "yellow";
    } else if (index === 5) {
        selection = "cyan";
    } else if (index === 6) {
        selection = "white";
    }
    let filePath = "/ext/" + selection + ".sub";
    let content = generateContent(selection);
    let success = storage.write(filePath, content);
    if (!success) {
        views.dialog.set("text", "Failed to write " + selection);
        gui.viewDispatcher.switchTo(views.dialog);
        return;
    }
    let result = subghz.transmitFile(filePath);
    if (result === true) {
        views.dialog.set("text", "Transmitted " + selection);
    } else {
        views.dialog.set("text", "Failed to transmit " + selection);
    }
    gui.viewDispatcher.switchTo(views.dialog);
}, gui, views);

eventLoop.subscribe(views.patterns.chosen, function(_sub, index, gui, views) {
    let selection;
    if (index === 0) {
        selection = "flash";
    } else if (index === 1) {
        selection = "crossfade";
    } else if (index === 2) {
        selection = "spark";
    } else if (index === 3) {
        selection = "auto";
    }
    views.dialog.set("text", "You selected pattern: " + selection);
    gui.viewDispatcher.switchTo(views.dialog);
}, gui, views);

eventLoop.subscribe(gui.viewDispatcher.navigation, function(_sub, _, gui, views, eventLoop) {
    if (gui.viewDispatcher.currentView === views.main) {
        eventLoop.stop();
    } else if (gui.viewDispatcher.currentView === views.colors ||
               gui.viewDispatcher.currentView === views.patterns) {
        gui.viewDispatcher.switchTo(views.main);
    } else if (gui.viewDispatcher.currentView === views.dialog) {
        if (lastMenu === "colors") {
            gui.viewDispatcher.switchTo(views.colors);
        } else if (lastMenu === "patterns") {
            gui.viewDispatcher.switchTo(views.patterns);
        } else {
            gui.viewDispatcher.switchTo(views.main);
        }
    }
}, gui, views, eventLoop);

eventLoop.subscribe(views.dialog.input, function(_sub, button, gui, views) {
    if (button === "center") {
        gui.viewDispatcher.switchTo(views.main);
    }
}, gui, views);

gui.viewDispatcher.switchTo(views.main);
eventLoop.run();
