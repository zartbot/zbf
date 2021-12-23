package main

import (
	"fmt"
	"net"

	"./geoipmap"
	"github.com/sirupsen/logrus"
)

func main() {
	src := "0.0.0.0:12345"
	listener, err := net.ListenPacket("udp", src)
	if err != nil {
		logrus.Fatal(err)
	}
	defer listener.Close()

	g, err := geoipmap.NewGeoIPCollector("./city.mmdb", "./asn.mmdb", 31.123, 111.11)
	if err != nil {
		logrus.Fatal(err)
	}

	for {
		buf := make([]byte, 2048)
		n, addr, err := listener.ReadFrom(buf)
		if err != nil {
			continue
		}
		go serve(g, addr, buf[:n])

	}
}

func serve(g *geoipmap.GeoIPCollector, addr net.Addr, buf []byte) {

	for i := 0; i < len(buf); i += 8 {

		src := net.IP(buf[i : i+4])
		dst := net.IP(buf[i+4 : i+8])

		s := g.Lookup(src)
		d := g.Lookup(dst)
		fmt.Printf("%s|%s|%s|%s|%s|%s\n", src, s.Country, s.ASN, dst, d.Country, d.ASN)
	}

}
