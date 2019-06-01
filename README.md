
# Caronte

> Caron, non ti crucciare: vuolsi così colà dove si puote ciò che si vuole, e più non dimandare.

_Caronte_ is a **Windows filter-driver** that lead poor IRP packets to completion. Caronte comes with _Virgilio_, a  **companion console application** that handle the scraped data.

_Virgilio_ is splitted in two component: 

* On-vm client application that push scrapped data to server outside vm.
* Server-side host application that store scrapped data to ransomware-safe database.

## Enviorment setup
...


## Windows I/O model overview 

Every operating system has an implicit or explicit I/O model for handling the flow of data to and from peripheral devices. One feature of the Microsoft Windows I/O model is its support for asynchronous I/O. In addition, the I/O model has the following general features:

-   The I/O manager presents a consistent interface to all kernel-mode drivers, including lowest-level, intermediate, and file system drivers. All I/O requests to drivers are sent as I/O request packets (IRPs).

-   I/O operations are layered. The I/O manager exports I/O system services, which user-mode protected subsystems call to carry out I/O operations on behalf of their applications and/or end users. The I/O manager intercepts these calls, sets up one or more IRPs, and routes them through possibly layered drivers to physical devices.

-   The I/O manager defines a set of standard routines, some required and others optional, that drivers can support. All drivers follow a relatively consistent implementation model, given the differences among peripheral devices and the differing functionality required of bus, function, filter, and file system drivers.

## Example of I/O request

![Example of I/O request](https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/images/2opendev.png)
1.  The subsystem calls an I/O system service to open a named file.

2.  The I/O manager calls the object manager to look up the named file and to help it resolve any symbolic links for the file object. It also calls the security reference monitor to check that the subsystem has the correct access rights to open that file object.

3.  If the volume is not yet mounted, the I/O manager suspends the open request temporarily and calls one or more file systems until one of them recognizes the file object as something it has stored on one of the mass-storage devices the file system uses. When the file system has mounted the volume, the I/O manager resumes the request.

4.  The I/O manager allocates memory for and initializes an IRP for the open request. To drivers, an open is equivalent to a "create" request.

5.  The I/O manager calls the file system driver, passing it the IRP. The file system driver accesses its I/O stack location in the IRP to determine what operation it must carry out, checks parameters, determines if the requested file is in cache, and, if not, sets up the next-lower driver's I/O stack location in the IRP.

6.  Both drivers process the IRP and complete the requested I/O operation, calling kernel-mode support routines supplied by the I/O manager and by other system components (not shown in the previous figure).

7.  The drivers return the IRP to the I/O manager with the I/O status block set in the IRP to indicate whether the requested operation succeeded or why it failed.

8.  The I/O manager gets the I/O status from the IRP, so it can return status information through the protected subsystem to the original caller.

9.  The I/O manager frees the completed IRP.

10.  The I/O manager returns a handle for the file object to the subsystem if the open operation was successful. If there was an error, it returns appropriate status to the subsystem.

After a subsystem successfully opens a file object that represents a data file, a device, or a volume, the subsystem uses the returned handle to identify the file object in subsequent requests for device I/O operations (usually read, write, or device I/O control requests). To make such a request, the subsystem calls I/O system services. The I/O manager routes these requests as IRPs sent to appropriate drivers.

## Mini-filter driver

The filter manager is a kernel-mode driver that conforms to the legacy file system filter model and exposes functionality commonly required in file system filter drivers. By taking advantage of this functionality, third-party developers can write minifilter drivers, which are simpler to develop than legacy file system filter drivers, thus shortening the development process while producing higher-quality, more robust drivers.

### Filter concepts
The filter manager is installed with Windows, but it becomes active only when a minifilter driver is loaded. The filter manager attaches to the file system stack for a target volume. A minifilter driver attaches to the file system stack indirectly, by registering with the filter manager for the I/O operations the minifilter driver chooses to filter.

A legacy filter driver's position in the file system I/O stack relative to other filter drivers is determined at system startup by its load order group. For example, an antivirus filter driver should be higher in the stack than a replication filter driver, so it can detect viruses and disinfect files before they are replicated to remote servers. Therefore, filter drivers in the FSFilter Anti-Virus load order group are loaded before filter drivers in the FSFilter Replication group. Each load order group has a corresponding system-defined class and class GUID used in the INF file for the filter driver.

Like legacy filter drivers, minifilter drivers attach in a particular order. However, the order of attachment is determined by a unique identifier called an _altitude_. The attachment of a minifilter driver at a particular altitude on a particular volume is called an _instance_ of the minifilter driver.

A minifilter driver's altitude ensures that the instance of the minifilter driver is always loaded at the appropriate location relative to other minifilter driver instances, and it determines the order in which the filter manager calls the minifilter driver to handle I/O. Altitudes are allocated and managed by Microsoft.

The following figure shows a simplified I/O stack with the filter manager and three minifilter drivers.

![enter image description here](https://docs.microsoft.com/en-us/windows-hardware/drivers/ifs/images/filter-manager-architecture-1.gif)

## Our model

Our model consist of two component, **Caronte**, the minifilter driver that intercepts all IRP packets and scrap all intresting data...
## Raw data to feature

Another important process is finding a good features starting by the scrapped data. This process...