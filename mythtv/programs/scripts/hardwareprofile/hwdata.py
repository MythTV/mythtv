
class myVendor(object):
    def __init__(self):
        self.name = ""
        self.num = ""
        self.devices = {}

class myDevice(object):
    def __init__(self):
        self.name = ""
        self.num = ""
        self.subvendors = {}
        self.vendor = ""

class DeviceMap:
    vendors = {}

    def __init__(self, bus='pci'):
        self.vendors['pci'] = self.device_map('pci')
        self.vendors['usb'] = self.device_map('usb')

    def device_map(self, bus='pci'):
        fns = ['/usr/share/%s.ids' % bus,
               '/usr/share/hwdata/%s.ids' % bus,
               '/usr/share/misc/%s.ids' % bus]
        for fn in fns:
            try:
                fo = open(fn, 'r')
                break
            except IOError:
                pass
        else:
            raise Exception('Hardware data not found')
	
        fo = open(fn, 'r')
        vendors = {}
        curvendor = None
        curdevice = None
        for line in fo.readlines():
            if line.startswith('#') or not line.strip():
                continue
            elif not line.startswith('\t'):
                curvendor = myVendor()
                try:
                    curvendor.num = int(line[0:4], 16)
                except:
                    continue
                curvendor.name = line[6:-1]
                vendors[curvendor.num] = curvendor
                continue

            elif line.startswith('\t\t'):
                line = line.replace('\t', '')
                thisdev = myDevice()
                try:
                    thisdev.vendor = int(line[0:4], 16)
                except:
                    continue
                try:
                    thisdev.num = int(line[5:9], 16)
                except:
                    continue
                thisdev.name = line[11:-1]
                subvend = myVendor()
                try:
                    subvend.num = thisdev.vendor
                except:
                    continue
                subvend.name = ""

                if not curdevice.subvendors.has_key(subvend.num):
                    curdevice.subvendors[subvend.num] = subvend
                    subvend.devices[thisdev.num] = thisdev
                else:
                    subvend = curdevice.subvendors[subvend.num]
                    subvend.devices[thisdev.num] = thisdev

                continue

            elif line.startswith('\t'):
                line = line.replace('\t', '')
                curdevice = myDevice()
                try:
                    curdevice.num = int(line[0:4], 16)
                except:
                    continue
                curdevice.name = line[6:-1]
                try:
                    curdevice.vendor = int(curvendor.num)
                except:
                    continue
                curvendor.devices[curdevice.num] = curdevice
                continue
            else:
                print line
                continue
        fo.close()
        # This introduces a bug, will fix later.
#        vendors[0] = myVendor()
#        vendors[0].name = 'N/A'
        return vendors

    def vendor(self, vend, subvend=None, alt='N/A', bus='pci'):
        try:
            vend = int(vend)
        except:
            return alt
        if vend == 0:
            return alt
        try:
            return self.vendors[bus][vend].devices[dev].subvendors[subvend].name + "a"
        except:
            try:
                return self.vendors[bus][vend].name
            except:
                return alt

    def device(self, vend, dev, subvend=None, subdevice=None, alt='N/A', bus='pci'):
        try:
            vend = int(vend)
        except:
            pass
        try:
            dev = int(dev)
        except:
            pass
        try:
            subvend = int(subvend)
        except:
            pass
        try:
            subdevice = int(subdevice)
        except:
            pass
        try:
            return self.vendors[bus][vend].devices[dev].name
        except:
            try:
                    self.vendors[bus][vend].devices[dev].name
            except:
                return alt

    def subdevice(self, vend, dev, subvend, subdevice, alt='N/A', bus='pci'):
#        return self.vendors[vend].devices[dev].name
        try:
            vend = int(vend)
        except:
            pass
        if vend == 0:
            return alt
        try:
            dev = int(dev)
        except:
            pass
        try:
            subvend = int(subvend)
        except:
            pass
        try:
            subdevice = int(subdevice)
        except:
            pass
        try:
            var = self.vendors[bus][vend].devices[dev].subvendors[subvend].devices[subdevice].name
            return var
        except:
            try:
                return self.vendors[bus][vend].devices[dev].name
            except:
                return alt
