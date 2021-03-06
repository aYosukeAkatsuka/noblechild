package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"strings"
	"time"

	"github.com/paypal/gatt"

	noblechild "github.com/shirou/noblechild"
)

var done = make(chan struct{})

func onStateChanged(d gatt.Device, s gatt.State) {
	fmt.Println("State:", s)
	switch s {
	case gatt.StatePoweredOn:
		fmt.Println("Scanning...")
		d.Scan([]gatt.UUID{}, false)
		return
	default:
		d.StopScanning()
	}
}

func onPeriphDiscovered(p gatt.Peripheral, a *gatt.Advertisement, rssi int) {
	id := strings.ToUpper(flag.Args()[0])
	fmt.Printf("discovered: %s", p.ID())
	if strings.ToUpper(p.ID()) != id {
		return
	}

	// Stop scanning once we've got the peripheral we're looking for.
	p.Device().StopScanning()

	fmt.Printf("\nPeripheral ID:%s, NAME:(%s)\n", p.ID(), p.Name())
	fmt.Println("  Local Name        =", a.LocalName)
	fmt.Println("  TX Power Level    =", a.TxPowerLevel)
	fmt.Println("  Manufacturer Data =", a.ManufacturerData)
	fmt.Println("  Service Data      =", a.ServiceData)
	fmt.Println("")

	p.Device().Connect(p)
}

func onPeriphConnected(p gatt.Peripheral, err error) {
	fmt.Println("Connected")
	defer p.Device().CancelConnection(p)

	s_uuid := gatt.MustParseUUID("ffe0")
	service, err := p.DiscoverServices([]gatt.UUID{s_uuid})
	if err != nil {
		fmt.Printf("Failed to discover services, err: %s\n", err)
		return
	}
	fmt.Println("ffe0 Service discoverd")
	c_uuid := gatt.MustParseUUID("ffe5")
	c, err := p.DiscoverCharacteristics([]gatt.UUID{c_uuid}, service[0])
	if err != nil {
		fmt.Printf("Failed to discover characteristics, err: %s\n", err)
		return
	}
	fmt.Println("ffe5 Characteristics discoverd")

	b, err := p.ReadCharacteristic(c[0])
	if err != nil {
		fmt.Printf("Failed to read characteristic, err: %s\n", err)
		return
	}
	fmt.Printf("    value         %x | %q\n", b, b)

	ra := []string{"\x00", "\xff"}
	ga := []string{"\x00", "\xff"}
	ba := []string{"\x00", "\xff"}

	for _, rr := range ra {
		for _, gg := range ga {
			for _, bb := range ba {
				led := []byte(rr + gg + bb + "\x00")
				fmt.Printf("      led         %x | %q\n", led, led)

				p.WriteCharacteristic(c[0], led, false)

				b, err := p.ReadCharacteristic(c[0])
				if err != nil {
					fmt.Printf("Failed to read characteristic, err: %s\n", err)
					return
				}
				fmt.Printf("    value         %x | %q\n", b, b)
				time.Sleep(2 * time.Second)
			}
		}
	}

	//	close(done)
}

func onPeriphDisconnected(p gatt.Peripheral, err error) {
	fmt.Println("Disconnected")
	close(done)
}

func main() {
	flag.Parse()
	if len(flag.Args()) != 1 {
		log.Fatalf("usage: %s [options] peripheral-id\n", os.Args[0])
	}

	var DefaultClientOptions = []gatt.Option{}

	d, err := noblechild.NewDevice(DefaultClientOptions...)
	if err != nil {
		log.Fatalf("Failed to open device, err: %s\n", err)
		return
	}

	// Register handlers.
	d.Handle(
		noblechild.PeripheralDiscovered(onPeriphDiscovered),
		noblechild.PeripheralConnected(onPeriphConnected),
		noblechild.PeripheralDisconnected(onPeriphDisconnected),
	)

	d.Init(onStateChanged)
	<-done
	fmt.Println("Done")

}
