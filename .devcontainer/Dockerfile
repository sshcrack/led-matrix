# Add custom NuGet source for vcpkg as vscode user
USER vscode
RUN mono `vcpkg fetch nuget | tail -n 1` sources add -Name sshcrack -Source https://nuget.sshcrack.me/v3/index.json
