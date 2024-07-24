.. _simplelink_connect_mesh_guide_uart_bridge:

SimpleLink Connect Mesh Feature Guide
#####################################

Configuring Mesh Network
************************

On the landing page, tap on the `Mesh Network` button. This will take you to the
mesh network page where you can add nodes and make configurations.

Adding a Network Key
====================
1. Tap on the menu button (three dots icon on the top right)
2. Tap `Configure network keys`
3. Tap `Add new key`
4. On the pop-up, tap `Save`

Adding an Application Key
=========================
1. Tap on the menu button (three dots icon on the top right)
2. Tap `Configure app keys`
3. Tap `Add new key`
4. On the pop-up, tap `Save`

Adding a Provisioner
====================

1. Tap on the menu button (three dots icon on the top right)
2. Tap `Provisioners`
3. Tap `Add new provisioner`
4. On the pop-up, confirm information and tap `Save`

Adding a Mesh Node
==================
1. In the Mesh Network, tap on the `Add Node` button
2. Tap the node you want to provision
3. Tap on the `Provision` button

Configuring a Mesh Node
***********************

Binding an Application Key to a Model
=====================================

1. In the Mesh Network, tap on the node you want to configure
2. At the bottom section, select the Element that contains the Model
   you want to configure
3. Select the Model
4. Under `Bind Application Key`, select the Application Key to bind to the model


Adding a Publish Address
========================
1. In the Mesh Network, tap on the node you want to configure
2. At the bottom section, select the Element that contains the Model
   you want to configure
3. Select the Model
4. On the right of `Publish`, tap the `Set publication` button
5. Select a Publish Address type (i.e., Unicast, Groups, etc.)
    - Unicast
        If you select `Unicast`, set the unicast address and tap `Apply`
    - Groups
        If you select `Groups`, you can either select from the existing ones or
        create a new group to publish to. If creating a new group, select
        `Create new group`, set the group name and the address and then tap
        `Apply`.

Adding a Subscription Address
=============================
1. In the Mesh Network, tap on the node you want to configure
2. At the bottom section, select the Element that contains the Model
   you want to configure
3. Select the Model
4. On the right of `Subscribe`, tap the `Subscribe` button
5. Select a Subscription Address type (i.e., Groups, All Proxies, etc.)
    - Groups
       If you select `Groups`, you can either select from the existing ones or
       create a new group to subscribe to. If creating a new group, select
       `Create new group to subscribe`, set the group name and the address and
       then tap `Apply`.
