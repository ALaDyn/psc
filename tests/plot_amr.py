#! /usr/bin/env python

from mpl_toolkits.mplot3d import axes3d
import matplotlib.pyplot as plt
import numpy as np

basename = "run"
mx = 8
my = 8
mz = 1
sw = 3
buf = 0
#patches = [0,1,2,4,7,11,12,13]
#patches = [5,6,9,10]
#patches = [0,1,2,4,7,11,12,13, 5,6,9,10]
patches = xrange(5)
times = xrange(0,32,1)

EX = 0
EY = 1
EZ = 2
HX = 3
HY = 4
HZ = 5

def read_patch(basename, fldname, time, p):
    filename = "%s.%06d_p%06d_%s.asc" % (basename, time, p, fldname)
    fld = np.loadtxt(filename)
    assert fld.shape[0] == (mx + 2 * sw) * (my + 2 * sw) * (mz + 2 * sw)
    nr_comps = fld.shape[1] - 3
    fld = fld.reshape(mz + 2 * sw, my + 2 * sw, mx + 2 * sw, 3 + nr_comps)
    crdcc = [fld[0,0,:,0], fld[0,:,0,1]]
    crdnc = []
    for crd in crdcc:
        tmpcc = np.empty(crd.size + 2)
        tmpcc[1:-1] = crd
        tmpcc[0] = 2 * tmpcc[1] - tmpcc[2]
        tmpcc[-1] = 2 * tmpcc[-2] - tmpcc[-3]
        crdnc.append(.5 * (tmpcc[:-2] + tmpcc[1:-1]))
    fld = fld[sw,:,:,3:]
    return fld, crdcc, crdnc

def plot_component(basename, fldname, time, m, **kwargs):
    for p in patches:
        fld, crdcc, crdnc = read_patch(basename, fldname, time, p)
        if m == EY:
            slx = slice(sw-buf, -(sw-buf))
            if sw-buf-1 == 0:
                sly = slice(sw-buf, None)
            else:
                sly = slice(sw-buf, -(sw-buf-1))
            crdcc = [c[slx] for c in crdcc]
            crdnc = [c[sly] for c in crdnc]
            X, Y = np.meshgrid(crdnc[0], crdcc[1])
        elif m == EZ:
            if sw-buf-1 == 0:
                slx = slice(sw-buf, None)
                sly = slice(sw-buf, None)
            else:
                slx = slice(sw-buf, -(sw-buf-1))
                sly = slice(sw-buf, -(sw-buf-1))
            X, Y = np.meshgrid(crdnc[0][slx], crdnc[1][sly])
        elif m == HZ:
            slx = slice(sw-buf, -(sw-buf))
            sly = slice(sw-buf, -(sw-buf))
            crdcc = [c[slx] for c in crdcc]
            X, Y = np.meshgrid(crdcc[0], crdcc[1])

        fld = fld[slx,sly,:]

        ax = plt.gca(projection='3d')
        ax.plot_wireframe(X, Y, fld[:,:,m], **kwargs)
        #ax.plot(X[2,:], fld[2,:,m], 'o-', **kwargs)
        # if buf > 0:
        #     ax.plot_wireframe(X[buf:-buf,buf:-buf], Y[buf:-buf,buf:-buf], fld[buf:-buf,buf:-buf,m], color='g')
        #ax.plot_wireframe(X, Y, np.sin(.5+2*np.pi*X)*np.cos(.5+2*np.pi*Y), color='g')

plt.ion()
for time in times:
    #plt.figure()
    plt.clf()
    plot_component(basename, "fld", time, EY, color='r')
    #plot_component("run", "fld", time, EZ, color='r')
    plot_component(basename, "fld", time, HZ, color='b')
    plt.draw()
    #plt.show()

plt.show()


