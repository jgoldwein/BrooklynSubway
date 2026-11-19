import unittest
from pathlib import Path
import overlay
from gerber_regions import region_signature


def region(points):
    return 'G36*\n'+'\n'.join(f'G01X{x}Y{y}D0{2 if i==0 else 1}*' for i,(x,y) in enumerate(points))+'\nG37*'


def gerber(*regions):
    return ('%FSLAX45Y45*%\n%MOMM*%\n%LPD*%\n'+'\n'.join(regions)+'\nM02*').encode()


class RegionTests(unittest.TestCase):
    def setUp(self):
        self.box=[(0,0),(10,0),(10,10),(0,10),(0,0)]
        self.other=[(20,0),(30,0),(30,10),(20,10),(20,0)]

    def test_start_direction_collinear_and_region_order_are_equivalent(self):
        traced=[(10,10),(10,5),(10,0),(0,0),(0,10),(10,10)]
        self.assertTrue(overlay.same_silkscreen_regions('top.GTO',gerber(region(self.box),region(self.other)),gerber(region(self.other),region(traced))))

    def test_shifted_removed_or_added_shape_is_rejected(self):
        a=gerber(region(self.box),region(self.other))
        for b in [gerber(region(self.box)),gerber(region(self.box),region([(x+1,y) for x,y in self.other])),gerber(region(self.box),region(self.other),region(self.box))]:
            self.assertFalse(overlay.same_silkscreen_regions('top.GTO',a,b))

    def test_hole_is_preserved(self):
        outer=region(self.box)[5:-5]
        hole=region([(3,3),(7,3),(7,7),(3,7),(3,3)])[5:-5]
        self.assertNotEqual(region_signature(outer),region_signature(outer+'\n'+hole))

    def test_other_layers_and_intervening_polarity_are_not_relaxed(self):
        a=gerber(region(self.box),region(self.other));b=gerber(region(self.other),region(self.box))
        self.assertFalse(overlay.same_silkscreen_regions('copper.GTL',a,b))
        b=b.replace(b'\nG36*',b'\n%LPC*%\nG36*',1)
        self.assertFalse(overlay.same_silkscreen_regions('top.GTO',a,b))

    def test_unsupported_geometry_and_open_contours_fail_closed(self):
        self.assertIsNone(region_signature('G02X10Y10D01*'))
        self.assertIsNone(region_signature('G01X0Y0D02*\nG01X10Y10D01*'))


if __name__=='__main__':unittest.main()
