import matplotlib.pyplot as plt
plt.ion()

class DynamicUpdate():
    #Suppose we know the x range

    def on_launch(self,min_x=0,max_x=1000,min_y=-1,max_y=1,shape='.'):
        #Set up plot
        self.figure, self.ax = plt.subplots()
        self.lines, = self.ax.plot([],[], shape)
        #Autoscale on unknown axis and known lims on the other
        self.ax.set_autoscaley_on(True)
        self.ax.set_ylim(min_y, max_y)
        self.ax.set_xlim(min_x, max_x)
        #Other stuff
        self.ax.grid()


    def update(self, xdata, ydata):
        #Update data (with the new _and_ the old points)
        self.lines.set_xdata(xdata)
        self.lines.set_ydata(ydata)
        #self.ax.set_ylim (0, ymax)
        #Need both of these in order to rescale
        self.ax.relim()
        self.ax.autoscale_view()
        #We need to draw *and* flush
        self.figure.canvas.draw()
        self.figure.canvas.flush_events()
