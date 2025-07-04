# Use minideb minimal debian distribution
FROM bitnami/minideb:latest

# Set environment variables to avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update package list and install necessary utilities
RUN apt-get update && apt-get install -y sudo

# Create a new user (e.g., "myuser") with a home directory
RUN useradd -m -s /bin/bash ssluser

# Set a password for the new user (change "password" as needed)
RUN echo "ssluser:sslpwd" | chpasswd

# Add the user to the sudo group for administrative privileges
RUN usermod -aG sudo ssluser

# Switch to the new user
USER ssluser

# Set the working directory to the new user's home
WORKDIR /home/ssluser

# Install required python packages
RUN echo 'sslpwd' | sudo -S install_packages python3 python3-pip
RUN pip3 install mysql-connector-python  --break-system-packages
RUN pip3 install paho-mqtt  --break-system-packages

# Copy the wrapper script into the container
COPY start_dbinjector.sh /home/ssluser/start_dbinjector.sh

# Copy the python dbinjector script into the container
COPY mqtt2db.py /home/ssluser/mqtt2db.py

# Make the wrapper script executable
# The chmod command changes file permissions; 
# +x makes the file executable.
RUN echo 'sslpwd' | sudo -S chmod +x /home/ssluser/start_dbinjector.sh

# Default command to keep the container running
CMD ["/home/ssluser/start_dbinjector.sh"]

