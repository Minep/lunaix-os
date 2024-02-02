
class SignalHelper:

    @staticmethod
    def get_sig_bitmap(sigbmp):
        if sigbmp == 0:
            return '<None>'
        v = []
        i = 0
        while sigbmp != 0:
            if sigbmp & 1 != 0:
                v.append(str(i))
            sigbmp = sigbmp >> 1
            i+=1
        return ",".join(v)