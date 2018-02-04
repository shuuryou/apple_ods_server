Apple used to allow sharing a CD or DVD drive connected to a Mac with another Mac. They killed the feature with macOS Catalina 10.15, because Apple being Apple, of course they would.

I once had an iMac and a Linux server with an optical disk drive built-in and needed to read a data CD on the Mac.

This quick hack thrown together in about 30 minutes implements just enough of the "Share DVDs and CDs on Mac" (ODS Server) feature to stream data DVDs and CDs from a Linux host to a Mac client.

It doesn't work with audio CDs (they do not provide a readable file in Linux) or video DVDs (because of CSS encryption).

This hack uses udev to determine if a disk is in the drive and then creates an MDNS (Bonjour) service using Avahi. The actual server is designed to be served by nginx via FastCGI.


**THIS IS NOT PRODUCTION QUALITY SOFTWARE!** I am dumping this code here for posterity to show that it was possible to do this, but the software here serves no purpose anymore. It's also a lot simpler just to connect an external DVD drive via USB.
