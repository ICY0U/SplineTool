import struct, numpy as np
def read_agt(p):
    b=open(p,'rb').read(); assert b[:4]==b'AGT1'; n=struct.unpack_from('<I',b,4)[0]; at=8; out=[]
    for _ in range(n):
        l=struct.unpack_from('<I',b,at)[0]; at+=4; name=b[at:at+l].decode(); at+=l
        nv=struct.unpack_from('<I',b,at)[0]; at+=4; v=np.frombuffer(b,'<f4',nv*3,at).reshape(-1,3); at+=nv*12
        nt=struct.unpack_from('<I',b,at)[0]; at+=4; t=np.frombuffer(b,'<u4',nt*3,at).reshape(-1,3); at+=nt*12
        out.append((name,v,t))
    return out
